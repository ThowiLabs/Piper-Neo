"""Gradio web UI for resilient Piper Neo fine-tuning on Kaggle."""

from __future__ import annotations

import argparse
import csv
import ctypes
import json
import os
import re
import shutil
import signal
import subprocess
import sys
import threading
import time
import urllib.parse
import zipfile
from pathlib import Path
from typing import Dict, List, Optional, Tuple

import gdown
import gradio as gr
import requests

# Kaggle's notebook can expose /dev/nvidia* while the MCP/Gradio process does
# not inherit the driver library search path. Preload libcuda explicitly so
# torch.cuda.is_available() sees the same GPUs as the notebook.
try:
    ctypes.CDLL("/usr/local/nvidia/lib64/libcuda.so.1", mode=ctypes.RTLD_GLOBAL)
except OSError:
    pass

import torch
from huggingface_hub import HfApi

KAGGLE_ROOT = Path(os.environ.get("PIPER_FINETUNE_ROOT", "/kaggle/working/piper_finetune"))
DEFAULT_BASE_URL = (
    "https://huggingface.co/datasets/rhasspy/piper-checkpoints/resolve/main/"
    "es/es_ES/davefx/medium/epoch%3D5629-step%3D1605020.ckpt"
)
DEFAULT_LANGUAGE = "es-419"
DEFAULT_SAMPLE_RATE = 22050


def _safe_project_name(name: str) -> str:
    name = re.sub(r"[^A-Za-z0-9._-]+", "-", (name or "piper-run").strip())
    name = name.strip(".-_")
    return (name or "piper-run")[:80]


def _project_dir(name: str) -> Path:
    return KAGGLE_ROOT / _safe_project_name(name)


def _other_training_pids(project: Path) -> list[int]:
    """Find existing Piper trainers across independently started Gradio servers."""
    dataset_dir = str((Path(project) / "training").resolve())
    current_pid = os.getpid()
    found = []
    proc = Path("/proc")
    if not proc.is_dir():
        return found
    for entry in proc.iterdir():
        if not entry.name.isdecimal():
            continue
        pid = int(entry.name)
        if pid == current_pid:
            continue
        try:
            argv = (entry / "cmdline").read_bytes().split(b"\x00")
            args = [item.decode("utf-8", "replace") for item in argv if item]
        except (OSError, PermissionError):
            continue
        if not ("-m" in args and "piper_train" in args):
            continue
        if "--dataset-dir" not in args:
            continue
        index = args.index("--dataset-dir")
        if index + 1 < len(args):
            try:
                candidate = str(Path(args[index + 1]).resolve())
            except (OSError, ValueError):
                continue
            if candidate == dataset_dir:
                found.append(pid)
    return sorted(found)


def _assert_no_external_training(project: Path) -> None:
    running = _other_training_pids(project)
    if running:
        raise gr.Error(
            f"Hay entrenamiento activo para este proyecto (PID {', '.join(map(str, running))}). "
            "No se permite preparar, preprocesar, borrar ni iniciar otro entrenamiento "
            "sobre el mismo dataset hasta que termine."
        )


def _upload_path(value) -> Optional[Path]:
    if value is None:
        return None
    if isinstance(value, (str, os.PathLike)):
        return Path(value)
    name = getattr(value, "name", None)
    return Path(name) if name else None


def _log_append(path: Path, message: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    stamp = time.strftime("%Y-%m-%d %H:%M:%S")
    with path.open("a", encoding="utf-8") as handle:
        handle.write(f"[{stamp}] {message}\n")


def _safe_extract_zip(zip_path: Path, destination: Path) -> None:
    destination.mkdir(parents=True, exist_ok=True)
    root = destination.resolve()
    with zipfile.ZipFile(zip_path) as archive:
        for member in archive.infolist():
            target = (destination / member.filename).resolve()
            if root != target and root not in target.parents:
                raise ValueError(f"ZIP contiene una ruta insegura: {member.filename}")
        archive.extractall(destination)


def _download_http(url: str, output: Path) -> Path:
    output.parent.mkdir(parents=True, exist_ok=True)
    with requests.get(url, stream=True, timeout=(20, 120), allow_redirects=True) as response:
        response.raise_for_status()
        with output.open("wb") as handle:
            for chunk in response.iter_content(chunk_size=8 * 1024 * 1024):
                if chunk:
                    handle.write(chunk)
    return output


def _download_source(url: str, output: Path) -> Path:
    url = (url or "").strip()
    if not url:
        raise ValueError("Falta la URL.")
    output.parent.mkdir(parents=True, exist_ok=True)

    if "drive.google.com" in url:
        result = gdown.download(url=url, output=str(output), quiet=False, fuzzy=True)
        if not result:
            raise RuntimeError("Google Drive no pudo descargar el archivo.")
        return Path(result)

    return _download_http(url, output)


def _hardlink_or_copy(source: Path, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.exists():
        return
    try:
        os.link(source, destination)
    except OSError:
        shutil.copy2(source, destination)


def _find_metadata(search_root: Path) -> Optional[Path]:
    preferred = list(search_root.rglob("metadata.csv"))
    if preferred:
        return preferred[0]
    candidates = [
        p
        for p in search_root.rglob("*")
        if p.is_file() and p.suffix.lower() in {".csv", ".txt"}
    ]
    return candidates[0] if candidates else None


def _build_wav_map(search_root: Path) -> Tuple[Dict[str, Path], List[str]]:
    wav_map: Dict[str, Path] = {}
    collisions: List[str] = []
    for wav in search_root.rglob("*"):
        if not wav.is_file() or wav.suffix.lower() != ".wav":
            continue
        if wav.name in wav_map and wav_map[wav.name] != wav:
            collisions.append(wav.name)
            continue
        wav_map[wav.name] = wav
    return wav_map, sorted(set(collisions))


def _normalize_metadata(
    metadata_source: Path,
    wav_map: Dict[str, Path],
    output_metadata: Path,
    output_wav_dir: Path,
) -> dict:
    output_wav_dir.mkdir(parents=True, exist_ok=True)
    output_metadata.parent.mkdir(parents=True, exist_ok=True)

    total_rows = 0
    written_rows = 0
    invalid_rows: List[str] = []
    missing_wavs: List[str] = []
    duplicate_audio_rows: List[str] = []
    seen_audio = set()

    with metadata_source.open("r", encoding="utf-8-sig", newline="") as source, output_metadata.open(
        "w", encoding="utf-8", newline=""
    ) as destination:
        reader = csv.reader(source, delimiter="|")
        writer = csv.writer(destination, delimiter="|", lineterminator="\n")

        for line_number, row in enumerate(reader, 1):
            if not row or not any(cell.strip() for cell in row):
                continue

            total_rows += 1
            if len(row) < 2:
                invalid_rows.append(f"línea {line_number}: menos de 2 columnas")
                continue

            audio_id = row[0].strip()
            text = row[-1].strip()

            if line_number == 1 and audio_id.lower() in {"id", "file", "filename", "audio", "wav"}:
                continue

            if not audio_id or not text:
                invalid_rows.append(f"línea {line_number}: audio o texto vacío")
                continue

            basename = Path(audio_id).name
            if not basename.lower().endswith(".wav"):
                basename += ".wav"

            if basename in seen_audio:
                duplicate_audio_rows.append(f"línea {line_number}: {basename}")
                continue

            wav = wav_map.get(basename)
            if wav is None:
                missing_wavs.append(f"línea {line_number}: {basename}")
                continue

            seen_audio.add(basename)
            _hardlink_or_copy(wav, output_wav_dir / basename)
            writer.writerow([basename, text])
            written_rows += 1

    return {
        "metadata_rows": total_rows,
        "usable_rows": written_rows,
        "invalid_rows": invalid_rows,
        "missing_wavs": missing_wavs,
        "duplicate_audio_rows": duplicate_audio_rows,
    }


def prepare_dataset(
    project_name: str,
    source_url: str,
    zip_upload,
    metadata_upload,
    reset_project: bool,
) -> str:
    project = _project_dir(project_name)
    _assert_no_external_training(project)
    if JOB.is_running() and JOB.project == project:
        raise gr.Error("No se puede reemplazar el dataset durante el entrenamiento.")
    source_dir = project / "source"
    input_dir = project / "input"
    training_dir = project / "training"

    if reset_project and project.exists():
        shutil.rmtree(project)

    source_dir.mkdir(parents=True, exist_ok=True)
    input_dir.mkdir(parents=True, exist_ok=True)
    training_dir.mkdir(parents=True, exist_ok=True)

    archive = _upload_path(zip_upload)
    if archive is None and (source_url or "").strip():
        archive = source_dir / "dataset.zip"
        _download_source(source_url, archive)

    if archive is None:
        raise gr.Error("Proporciona una URL de dataset o sube un ZIP.")
    if not archive.exists():
        raise gr.Error(f"No existe el archivo del dataset: {archive}")

    extracted = source_dir / "extracted"
    if extracted.exists():
        shutil.rmtree(extracted)
    extracted.mkdir(parents=True, exist_ok=True)

    if zipfile.is_zipfile(archive):
        _safe_extract_zip(archive, extracted)
    elif archive.suffix.lower() == ".wav":
        shutil.copy2(archive, extracted / archive.name)
    else:
        raise gr.Error("El dataset debe ser un ZIP.")

    metadata = _upload_path(metadata_upload) or _find_metadata(extracted)
    if metadata is None or not metadata.exists():
        raise gr.Error(
            "No encontré metadata.csv dentro del ZIP. Sube el CSV/metadata por separado."
        )

    wav_map, wav_collisions = _build_wav_map(extracted)
    if not wav_map:
        raise gr.Error("No encontré archivos WAV dentro del dataset.")

    output_metadata = input_dir / "metadata.csv"
    output_wav_dir = input_dir / "wav"
    if output_wav_dir.exists():
        shutil.rmtree(output_wav_dir)
    output_wav_dir.mkdir(parents=True, exist_ok=True)

    result = _normalize_metadata(metadata, wav_map, output_metadata, output_wav_dir)
    result["wav_files_in_archive"] = len(wav_map)
    result["wav_name_collisions"] = wav_collisions
    result["project_dir"] = str(project)

    (project / "dataset_validation.json").write_text(
        json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8"
    )

    summary = [
        f"Proyecto: {project}",
        f"WAV únicos detectados: {len(wav_map)}",
        f"Filas metadata leídas: {result['metadata_rows']}",
        f"Filas utilizables: {result['usable_rows']}",
        f"Duplicados descartados: {len(result['duplicate_audio_rows'])}",
        f"WAV faltantes: {len(result['missing_wavs'])}",
        f"Filas inválidas: {len(result['invalid_rows'])}",
    ]
    if result["duplicate_audio_rows"]:
        summary.append(
            "Duplicados (primeros 20): "
            + ", ".join(result["duplicate_audio_rows"][:20])
        )
    if result["missing_wavs"]:
        summary.append(
            "Faltantes (primeros 20): " + ", ".join(result["missing_wavs"][:20])
        )
    if wav_collisions:
        summary.append(
            "Nombres WAV repetidos dentro del ZIP: " + ", ".join(wav_collisions[:20])
        )

    return "\n".join(summary)


def _tail(path: Path, lines: int = 80) -> str:
    if not path.exists():
        return ""
    with path.open("r", encoding="utf-8", errors="replace") as handle:
        data = handle.readlines()
    return "".join(data[-lines:])


def preprocess_dataset(
    project_name: str,
    language: str,
    sample_rate: int,
    max_workers: int,
) -> str:
    project = _project_dir(project_name)
    _assert_no_external_training(project)
    if JOB.is_running() and JOB.project == project:
        raise gr.Error("No se puede preprocesar durante el entrenamiento.")
    input_dir = project / "input"
    training_dir = project / "training"
    log_path = project / "preprocess.log"

    if not (input_dir / "metadata.csv").exists():
        raise gr.Error("Primero prepara/valida el dataset.")

    training_dir.mkdir(parents=True, exist_ok=True)
    command = [
        sys.executable,
        "-m",
        "piper_train.preprocess",
        "--language",
        (language or DEFAULT_LANGUAGE).strip(),
        "--input-dir",
        str(input_dir),
        "--output-dir",
        str(training_dir),
        "--dataset-format",
        "ljspeech",
        "--single-speaker",
        "--sample-rate",
        str(int(sample_rate)),
        "--max-workers",
        str(max(1, int(max_workers))),
    ]

    with log_path.open("w", encoding="utf-8") as log_file:
        process = subprocess.run(
            command,
            stdout=log_file,
            stderr=subprocess.STDOUT,
            text=True,
            env={**os.environ, "NUMBA_CACHE_DIR": str(project / ".numba_cache")},
        )

    tail = _tail(log_path, 100)
    if process.returncode != 0:
        raise gr.Error(f"Preprocess falló (código {process.returncode}).\n\n{tail}")

    config = training_dir / "config.json"
    dataset = training_dir / "dataset.jsonl"
    if not config.exists() or not dataset.exists():
        raise gr.Error("Preprocess terminó sin generar config.json/dataset.jsonl.")

    rows = sum(1 for line in dataset.open("r", encoding="utf-8") if line.strip())
    return (
        f"Preprocess completado.\nIdioma: {language}\nSample rate: {sample_rate}\n"
        f"Utterances procesadas: {rows}\nConfig: {config}\nDataset: {dataset}\n\n"
        f"Últimas líneas del log:\n{tail}"
    )


def _download_checkpoint(url: str, project: Path, folder: str = "base") -> Path:
    url = (url or "").strip()
    if not url:
        raise ValueError("Falta URL del checkpoint base.")
    parsed = urllib.parse.urlparse(url)
    basename = urllib.parse.unquote(Path(parsed.path).name)
    if not basename or not basename.endswith(".ckpt"):
        basename = "base.ckpt"
    destination = project / folder / basename
    if destination.exists() and destination.stat().st_size > 0:
        return destination
    return _download_source(url, destination)


def _checkpoint_paths(project: Path) -> List[Path]:
    training = project / "training"
    return sorted(
        training.glob("**/*.ckpt"),
        key=lambda p: (p.stat().st_mtime_ns, p.name),
    )


def _load_sync_manifest(project: Path) -> dict:
    path = project / ".hf_synced.json"
    if not path.exists():
        return {}
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except Exception:
        return {}


def _save_sync_manifest(project: Path, data: dict) -> None:
    (project / ".hf_synced.json").write_text(
        json.dumps(data, indent=2, ensure_ascii=False), encoding="utf-8"
    )


def sync_to_huggingface(
    project_name: str,
    repo_id: str,
    token: str,
    private: bool,
    remote_prefix: str,
) -> str:
    """Validate every checkpoint before pushing an original-named HF backup."""
    from .hf_backup import sync_verified_checkpoints

    token = (token or os.environ.get("HF_TOKEN", "")).strip()
    if not (repo_id or "").strip():
        raise gr.Error("Falta el repo_id de Hugging Face.")
    if not token:
        raise gr.Error("Falta el token HF (o configura HF_TOKEN).")

    from .hf_watcher import _SYNC_LOCK
    with _SYNC_LOCK:
        report = sync_verified_checkpoints(
            _project_dir(project_name),
            repo_id=repo_id,
            token=token,
            private=bool(private),
            remote_prefix="",
        )
    message = (
        f"Repositorio de respaldo: {report['repo_id']}\n"
        f"Checkpoints NUEVOS respaldados en HF: {len(report['uploaded'])}\n"
        f"Checkpoints que YA EXISTEN remotamente: {len(report['skipped'])}\n"
        f"Fallos/rechazados (no se subieron): {len(report['failed'])}\n"
        f"config.json: {report.get('config_status', 'sin comprobar')}"
    )
    if report["uploaded"]:
        message += "\nNuevos respaldos: " + ", ".join(report["uploaded"])
    if report["skipped"]:
        message += "\nYa estaban en HF: " + ", ".join(report["skipped"][:15])
    for error in report["failed"]:
        message += f"\nNO SUBIDO {error['file']}: {error['reason']}"
    _log_append(_project_dir(project_name) / "hf_backup.log", message.replace("\n", " | "))
    return message

class TrainingJob:
    def __init__(self) -> None:
        self.lock = threading.RLock()
        self.process: Optional[subprocess.Popen] = None
        self.project: Optional[Path] = None
        self.log_path: Optional[Path] = None
        self.log_handle = None
        self.sync_thread: Optional[threading.Thread] = None
        self.sync_stop = threading.Event()

    def is_running(self) -> bool:
        with self.lock:
            return self.process is not None and self.process.poll() is None

    def start(self, command: List[str], project: Path, sync_config: Optional[dict]) -> None:
        with self.lock:
            if self.is_running():
                raise RuntimeError("Ya existe un entrenamiento en ejecución.")
            other_pids = _other_training_pids(project)
            if other_pids:
                raise RuntimeError(
                    f"El proyecto ya está entrenando en PID(s) {other_pids}. "
                    "Para evitar daños, no se inicia otra instancia."
                )

            self.project = project
            self.log_path = project / "train.log"
            self.log_path.parent.mkdir(parents=True, exist_ok=True)
            self.log_handle = self.log_path.open("a", encoding="utf-8")
            self.log_handle.write("\n=== START TRAINING ===\n")
            self.log_handle.write("COMMAND: " + " ".join(command) + "\n")
            self.log_handle.flush()

            env = {
                **os.environ,
                "NUMBA_CACHE_DIR": str(project / ".numba_cache"),
                "PYTHONUNBUFFERED": "1",
                "LD_LIBRARY_PATH": (
                    "/usr/local/nvidia/lib64:"
                    + os.environ.get("LD_LIBRARY_PATH", "")
                ).rstrip(":"),
            }
            self.process = subprocess.Popen(
                command,
                stdout=self.log_handle,
                stderr=subprocess.STDOUT,
                text=True,
                env=env,
                cwd=str(project),
            )

            self.sync_stop.clear()
            if sync_config and sync_config.get("repo_id"):
                self.sync_thread = threading.Thread(
                    target=self._sync_loop,
                    args=(sync_config,),
                    daemon=True,
                )
                self.sync_thread.start()

    def _sync_loop(self, sync_config: dict) -> None:
        interval = max(30, int(sync_config.get("interval", 120)))
        while not self.sync_stop.wait(interval):
            try:
                sync_to_huggingface(
                    sync_config["project_name"],
                    sync_config["repo_id"],
                    sync_config.get("token", ""),
                    sync_config.get("private", True),
                    sync_config.get("remote_prefix", ""),
                )
            except Exception as exc:
                if self.project:
                    _log_append(self.project / "hf_sync.log", f"ERROR: {exc}")
            if not self.is_running():
                break

        try:
            sync_to_huggingface(
                sync_config["project_name"],
                sync_config["repo_id"],
                sync_config.get("token", ""),
                sync_config.get("private", True),
                sync_config.get("remote_prefix", ""),
            )
        except Exception as exc:
            if self.project:
                _log_append(self.project / "hf_sync.log", f"FINAL ERROR: {exc}")

    def stop(self) -> str:
        with self.lock:
            if not self.is_running():
                return "No hay entrenamiento activo."
            assert self.process is not None
            self.sync_stop.set()
            try:
                self.process.send_signal(signal.SIGTERM)
                self.process.wait(timeout=15)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait(timeout=5)
            code = self.process.returncode
            if self.log_handle:
                self.log_handle.flush()
                self.log_handle.close()
                self.log_handle = None
            return f"Entrenamiento detenido. Código de salida: {code}"

    def status(self) -> Tuple[str, List[str]]:
        with self.lock:
            process = self.process
            project = self.project
            log_path = self.log_path

        if project is None:
            return "Sin entrenamiento iniciado en esta sesión.", []

        running = process is not None and process.poll() is None
        code = None if running or process is None else process.returncode
        checkpoints = _checkpoint_paths(project)

        status = [
            f"Proyecto: {project}",
            f"Estado: {'ENTRENANDO' if running else 'DETENIDO/FINALIZADO'}",
        ]
        if code is not None:
            status.append(f"Código de salida: {code}")
        status.append(f"Checkpoints encontrados: {len(checkpoints)}")
        if checkpoints:
            status.append(f"Último: {checkpoints[-1]}")

        log_tail = _tail(log_path, 100) if log_path else ""
        if log_tail:
            status.extend(["", "--- train.log ---", log_tail])

        backup_log = _tail(project / "hf_backup.log", 30)
        if not backup_log:
            backup_log = _tail(project / "hf_sync.log", 15)
        if backup_log:
            status.extend(["", "--- Respaldo HF ---", backup_log])

        downloads = []
        config_path = project / "training" / "config.json"
        if config_path.exists():
            downloads.append(str(config_path))
        downloads.extend(str(p) for p in checkpoints)
        return "\n".join(status), downloads


JOB = TrainingJob()


def start_training(
    project_name: str,
    mode: str,
    base_checkpoint_url: str,
    base_checkpoint_upload,
    resume_checkpoint_upload,
    resume_checkpoint_url: str,
    batch_size: int,
    max_epochs: int,
    checkpoint_epochs: int,
    checkpoint_minutes: float,
    max_phoneme_ids: int,
    accelerator: str,
    hf_repo_id: str,
    hf_token: str,
    hf_private: bool,
    hf_remote_prefix: str,
    hf_sync_interval: int,
    hf_auto_backup: bool,
    resume_source: str,
    resume_hf_repo: str,
    resume_hf_checkpoint: str,
) -> str:
    project = _project_dir(project_name)
    _assert_no_external_training(project)
    training = project / "training"
    if not (training / "config.json").exists() or not (training / "dataset.jsonl").exists():
        raise gr.Error("Falta preprocess: no existen training/config.json y dataset.jsonl.")

    accel = (accelerator or "auto").lower()
    cuda = torch.cuda.is_available()
    if accel == "gpu" and not cuda:
        raise gr.Error("Se solicitó GPU pero torch.cuda.is_available() es False.")
    if accel == "auto":
        accel = "gpu" if cuda else "cpu"

    command = [
        sys.executable,
        "-m",
        "piper_train",
        "--dataset-dir",
        str(training),
        "--quality",
        "medium",
        "--accelerator",
        accel,
        "--devices",
        "1",
        "--batch-size",
        str(max(1, int(batch_size))),
        "--validation-split",
        "0.0",
        "--num-test-examples",
        "0",
        "--max_epochs",
        str(max(1, int(max_epochs))),
        "--precision",
        "32",
        "--default_root_dir",
        str(training),
    ]

    if float(checkpoint_minutes) > 0:
        command.extend(["--checkpoint-minutes", str(float(checkpoint_minutes))])
    else:
        command.extend(["--checkpoint-epochs", str(max(1, int(checkpoint_epochs)))])

    if int(max_phoneme_ids) > 0:
        command.extend(["--max-phoneme-ids", str(int(max_phoneme_ids))])

    if mode == "Resume de corrida":
        if resume_source == "Hugging Face: último válido":
            from .hf_resume import prepare_resume_from_hf
            repo = (resume_hf_repo or "").strip() or (hf_repo_id or "").strip()
            if not repo:
                raise gr.Error("Escribe el repo HF de origen, p. ej. usuario/capibara.")
            token = (hf_token or os.environ.get("HF_TOKEN", "")).strip()
            try:
                prepared = prepare_resume_from_hf(
                    project=project, repo_name=repo, token=token,
                    selected_checkpoint=resume_hf_checkpoint,
                    max_epochs=int(max_epochs),
                )
            except Exception as error:
                raise gr.Error(f"No se pudo recuperar HF: {error}") from error
            resume = Path(prepared["download_path"])
            init_description = (
                f"HF {prepared['repository']}/{prepared['filename']} "
                f"(epoch={prepared['epoch']}, global_step={prepared['global_step']}, "
                f"SHA256={prepared['sha256'][:12]}...). "
                f"Descargado en {resume}"
            )
        elif resume_source == "Subir checkpoint o URL":
            resume = _upload_path(resume_checkpoint_upload)
            if resume is None and (resume_checkpoint_url or "").strip():
                resume = _download_checkpoint(
                    resume_checkpoint_url.strip(), project, folder="resume"
                )
            if resume is None:
                raise gr.Error("Sube el checkpoint o proporciona una URL para Resume.")
            init_description = f"resume={resume}"
        else:
            checkpoints = _checkpoint_paths(project)
            if not checkpoints:
                raise gr.Error(
                    "No hay checkpoint local. Selecciona Hugging Face, sube uno o "
                    "descarga uno desde una URL."
                )
            resume = checkpoints[-1]
            init_description = f"resume={resume}"
        command.extend(["--resume_from_checkpoint", str(resume)])
    else:
        uploaded = _upload_path(base_checkpoint_upload)
        if uploaded is not None:
            base = project / "base" / uploaded.name
            base.parent.mkdir(parents=True, exist_ok=True)
            if uploaded.resolve() != base.resolve():
                shutil.copy2(uploaded, base)
        else:
            base = _download_checkpoint(base_checkpoint_url or DEFAULT_BASE_URL, project)
        command.extend(["--init-from-checkpoint", str(base)])
        init_description = f"init={base}"

    token = (hf_token or os.environ.get("HF_TOKEN", "")).strip()
    repo_id = (hf_repo_id or "").strip()
    if hf_auto_backup and repo_id:
        if not token:
            raise gr.Error("Activaste subida automática pero falta el token HF.")
        # Verify credentials and namespace before starting training.
        from .hf_backup import resolve_repo_id
        repo_id = resolve_repo_id(repo_id, token)

    try:
        # Centralize upload tracking in the independent watcher. No duplicate
        # in-training backup threads competing over the same checkpoint.
        JOB.start(command, project, None)
    except RuntimeError as exc:
        raise gr.Error(str(exc)) from exc

    sync_state = "desactivado"
    if hf_auto_backup and repo_id:
        try:
            sync_state = activate_backup(
                project_name, repo_id, token, hf_private, hf_sync_interval
            )
        except Exception as error:
            sync_state = f"No se pudo activar HF: {error}. Usa Activar respaldo."

    return (
        f"Entrenamiento iniciado.\nModo: {mode} ({init_description})\n"
        f"Accelerator: {accel} (CUDA visible={cuda})\nBatch size: {batch_size}\n"
        f"Max epochs: {max_epochs}\n"
        f"Checkpoint: {'cada ' + str(checkpoint_minutes) + ' min' if float(checkpoint_minutes) > 0 else 'cada ' + str(checkpoint_epochs) + ' epoch(s)'}\n"
        f"Respaldo incremental HF: {sync_state}\n"
        f"Log: {project / 'train.log'}"
    )


def inspect_hf_resume(repo_name: str, backup_repo: str, token: str):
    """Show remote history and offer all version names as an override."""
    from .hf_resume import ROOT_SELECTION, list_remote_versions

    auth_token = (token or os.environ.get("HF_TOKEN", "")).strip()
    try:
        repo_id, versions = list_remote_versions(
            (repo_name or "").strip() or (backup_repo or "").strip(), auth_token
        )
    except Exception as error:
        raise gr.Error(f"No se pudo leer el repo de HF: {error}") from error
    label = [
        f"Repo: {repo_id}",
        f"Checkpoints encontrados en raíz: {len(versions)}",
        f"Más reciente por global_step: {versions[0]}",
        "Selecciona Automático para elegir el más reciente que pase la validación, "
        "o elige un checkpoint específico.",
    ]
    label.extend(f"  {i+1}. {name}" for i, name in enumerate(versions[:30]))
    return (
        gr.update(
            choices=[ROOT_SELECTION] + versions,
            value=ROOT_SELECTION,
        ),
        "\n".join(label),
    )


def training_running(project_name: str) -> bool:
    project = _project_dir(project_name)
    return (JOB.project == project and JOB.is_running()) or bool(
        _other_training_pids(project)
    )


def training_buttons(project_name: str):
    """Always use kernel process state, not stale browser state."""
    running = training_running(project_name)
    if running:
        pids = _other_training_pids(_project_dir(project_name))
        message = (
            "Entrenamiento EN CURSO. El botón Iniciar está oculto. "
            + (f"PID(s): {', '.join(map(str, pids))}" if pids else "Proceso local activo")
        )
    else:
        message = "Sin entrenamiento activo para este proyecto. Puedes iniciar."
    return gr.update(visible=not running), gr.update(visible=running), message


def stop_training(project_name: str) -> str:
    """Stop only the selected project's trainer, even across Gradio instances.

    Never signal DataLoader children: identify the root trainer whose parent PID
    is not another Piper trainer in the same project. No side effects except
    when a real user clicks Detener.
    """
    project = _project_dir(project_name)
    if JOB.project == project and JOB.is_running():
        return JOB.stop()
    matching = _other_training_pids(project)
    if not matching:
        return "No hay entrenamiento activo para este proyecto."
    roots = []
    for pid in matching:
        try:
            stat = (Path("/proc") / str(pid) / "stat").read_text()
            ppid = int(stat.rsplit(")", 1)[1].split()[1])
        except (OSError, ValueError, IndexError):
            continue
        if ppid not in matching:
            roots.append(pid)
    if len(roots) != 1:
        raise gr.Error(
            f"Se detectaron {len(roots)} procesos principales: "
            "no puedo detenerlos con seguridad automáticamente."
        )
    root_pid = roots[0]
    # Re-check the actual command line immediately before sending a signal.
    if root_pid not in _other_training_pids(project):
        raise gr.Error("El proceso cambió de estado; actualiza el panel.")
    try:
        os.kill(root_pid, signal.SIGINT)
    except ProcessLookupError:
        return "El entrenamiento ya terminó."
    except PermissionError as error:
        raise gr.Error("No hay permisos para detener el entrenamiento.") from error
    return (
        f"Se solicitó una detención controlada al trainer PID {root_pid} "
        f"del proyecto {project.name}. Espera a que finalice."
    )


def refresh_status(project_name: str) -> Tuple[str, List[str]]:
    project = _project_dir(project_name)
    if JOB.project == project:
        return JOB.status()

    checkpoints = _checkpoint_paths(project) if project.exists() else []
    status = [
        f"Proyecto: {project}",
        (
            "Entrenamiento activo en otro Gradio: PID(s) "
            + ", ".join(map(str, _other_training_pids(project)))
            if _other_training_pids(project)
            else "Sin entrenamiento gestionado por esta instancia de Gradio."
        ),
        f"Checkpoints encontrados: {len(checkpoints)}",
    ]
    log = _tail(project / "train.log", 100)
    if log:
        status.extend(["", "--- train.log ---", log])
    downloads = []
    config_path = project / "training" / "config.json"
    if config_path.exists():
        downloads.append(str(config_path))
    downloads.extend(str(p) for p in checkpoints)
    return "\n".join(status), downloads


def inference_choices(project_name: str):
    from .inference_lab import find_checkpoints
    project = _project_dir(project_name)
    choices = [str(p) for p in find_checkpoints(project)]
    return gr.update(choices=choices, value=choices[0] if choices else None)


def gradio_synthesize(
    project_name, checkpoint_path, checkpoint_upload, config_upload,
    text, language, length_scale, noise_scale, noise_w,
):
    from .inference_lab import synthesize
    try:
        return synthesize(
            _project_dir(project_name), checkpoint_path, checkpoint_upload,
            config_upload, text, language, length_scale, noise_scale, noise_w
        )
    except Exception as error:
        raise gr.Error(f"No se pudo generar voz: {error}") from error


def gradio_export_onnx(
    project_name, checkpoint_path, checkpoint_upload, config_upload,
):
    from .inference_lab import export_onnx
    try:
        return export_onnx(
            _project_dir(project_name), checkpoint_path, checkpoint_upload,
            config_upload,
        )
    except Exception as error:
        raise gr.Error(f"No se pudo exportar ONNX: {error}") from error


def activate_backup(project_name, repo_id, token, private, interval):
    from .hf_watcher import BACKUP_WATCHER
    try:
        return BACKUP_WATCHER.start(
            _project_dir(project_name), repo_id, token, bool(private), int(interval)
        )
    except (ValueError, OSError) as error:
        raise gr.Error(str(error)) from error


def backup_status():
    from .hf_watcher import BACKUP_WATCHER
    return BACKUP_WATCHER.status()


def disable_backup():
    from .hf_watcher import BACKUP_WATCHER
    return BACKUP_WATCHER.stop()


def save_ui_preferences(project_name: str, *values) -> str:
    """Persist user-adjusted controls, but NEVER passwords/uploads."""
    from .studio_state import FIELDS, save_preferences

    prefs = dict(zip(FIELDS, values))
    save_preferences(KAGGLE_ROOT, _safe_project_name(project_name), prefs)
    return f"Preferencias guardadas para {_safe_project_name(project_name)}. El token HF nunca se guarda."


def load_ui_preferences(project_name=None, include_name=False):
    """Recover preferences and project artifacts after refresh/reconnect."""
    from .studio_state import FIELDS, load_preferences, save_preferences

    name, prefs = load_preferences(
        KAGGLE_ROOT, _safe_project_name(project_name) if project_name else None
    )
    if project_name:
        save_preferences(KAGGLE_ROOT, name, {})
    current_status, current_files = refresh_status(name)
    checkpoint_choice = inference_choices(name)
    summary = (
        f"Proyecto restaurado: {name}. Configuración guardada en disco. "
        "Dataset, logs, checkpoints y entrenamiento siguen independientes del navegador. "
        "Por seguridad el token HF no se guarda: usa HF_TOKEN del entorno o vuelve a ingresarlo."
    )
    values = [prefs[key] for key in FIELDS]
    backup_info = backup_status()
    start_state, stop_state, train_notice = training_buttons(name)
    if include_name:
        return (
            name, *values, current_status, current_files,
            checkpoint_choice, summary, backup_info,
            start_state, stop_state, train_notice,
        )
    return (
        *values, current_status, current_files,
        checkpoint_choice, summary, backup_info,
        start_state, stop_state, train_notice,
    )


def build_ui() -> gr.Blocks:
    with gr.Blocks(title="Piper Neo Fine-tune — Kaggle") as demo:
        gr.Markdown(
            "# Piper Neo Fine-tune — Kaggle\n"
            "Flujo reproducible para dataset → preprocess → fine-tune → checkpoints → backup HF.\n\n"
            "**Predeterminado:** español latinoamericano es-419, 22,050 Hz, "
            "davefx-medium como pesos base. Fine-tune y Resume son operaciones separadas."
        )

        with gr.Tab("1. Dataset"):
            project_name = gr.Textbox(value="capibara", label="Nombre del proyecto")
            preferences_status = gr.Textbox(
                label="Estado recuperado al recargar",
                interactive=False, lines=3,
            )
            source_url = gr.Textbox(
                label="URL ZIP (Google Drive público o enlace directo)",
                placeholder="https://drive.google.com/file/d/.../view",
            )
            zip_upload = gr.File(label="O sube el ZIP", file_types=[".zip"], type="filepath")
            metadata_upload = gr.File(
                label="CSV/metadata (opcional si ya viene dentro del ZIP)",
                file_types=[".csv", ".txt"],
                type="filepath",
            )
            reset_project = gr.Checkbox(
                value=False,
                label="Recrear el proyecto (borra el proyecto existente)",
            )
            prepare_btn = gr.Button("Preparar y validar dataset", variant="primary")
            dataset_report = gr.Textbox(label="Validación", lines=12)
            prepare_btn.click(
                prepare_dataset,
                inputs=[project_name, source_url, zip_upload, metadata_upload, reset_project],
                outputs=dataset_report,
            )

        with gr.Tab("2. Preprocess"):
            language = gr.Textbox(value=DEFAULT_LANGUAGE, label="Idioma eSpeak")
            sample_rate = gr.Number(value=DEFAULT_SAMPLE_RATE, precision=0, label="Sample rate")
            max_workers = gr.Number(
                value=max(1, min(4, (os.cpu_count() or 2) - 1)),
                precision=0,
                label="Workers",
            )
            preprocess_btn = gr.Button("Preprocesar", variant="primary")
            preprocess_report = gr.Textbox(label="Resultado", lines=16)
            preprocess_btn.click(
                preprocess_dataset,
                inputs=[project_name, language, sample_rate, max_workers],
                outputs=preprocess_report,
            )

        with gr.Tab("3. Entrenamiento"):
            mode = gr.Radio(
                ["Fine-tune desde modelo base", "Resume de corrida"],
                value="Fine-tune desde modelo base",
                label="Modo",
            )
            base_checkpoint_url = gr.Textbox(value=DEFAULT_BASE_URL, label="Checkpoint base URL")
            base_checkpoint_upload = gr.File(
                label="O sube checkpoint base (.ckpt)",
                file_types=[".ckpt"],
                type="filepath",
            )
            resume_checkpoint_upload = gr.File(
                label="Checkpoint para Resume (opcional; usa el último local si se deja vacío)",
                file_types=[".ckpt"],
                type="filepath",
            )
            resume_checkpoint_url = gr.Textbox(
                label="O URL directa/Drive de checkpoint para Resume",
                placeholder="https://huggingface.co/.../epoch=...ckpt",
            )
            gr.Markdown(
                "### Reanudar desde Hugging Face (checkpoint + optimizadores)\n"
                "Se detecta el mayor **global_step**, no el nombre alfabético; "
                "se comprueba config.json, tamaño, SHA-256 y estructura del checkpoint. "
                "Si el último está dañado, prueba uno anterior. "
                "**Usa solo repositorios de confianza:** CKPT usa pickle."
            )
            resume_source = gr.Radio(
                ["Local: último checkpoint", "Subir checkpoint o URL",
                 "Hugging Face: último válido"],
                value="Local: último checkpoint",
                label="Origen para reanudar",
            )
            resume_hf_repo = gr.Textbox(
                value="",
                label="Repositorio HF de origen (vacío = usar el repo de backup)",
                placeholder="usuario/mi-voz-piper o mi-voz-piper",
            )
            with gr.Row():
                hf_versions_btn = gr.Button("Consultar checkpoints HF")
                resume_hf_checkpoint = gr.Dropdown(
                    choices=["Automático: último checkpoint válido"],
                    value="Automático: último checkpoint válido",
                    label="Versión para reanudar (automático = última válida)",
                    allow_custom_value=True,
                )
            hf_versions_report = gr.Textbox(
                label="Checkpoints disponibles en HF", lines=7, interactive=False
            )
            with gr.Row():
                batch_size = gr.Number(value=16, precision=0, label="Batch size")
                max_epochs = gr.Number(value=1000, precision=0, label="Max epochs")
                checkpoint_epochs = gr.Number(
                    value=5, precision=0, label="Fallback: checkpoint cada N epochs"
                )
                checkpoint_minutes = gr.Number(
                    value=15,
                    precision=1,
                    label="Checkpoint cada N minutos (0 = usar epochs)",
                )
                max_phoneme_ids = gr.Number(
                    value=400, precision=0, label="Máx. phoneme IDs (0=sin límite)"
                )
            accelerator = gr.Dropdown(["auto", "gpu", "cpu"], value="auto", label="Accelerator")

            gr.Markdown(
                "### Respaldo incremental a Hugging Face\n"
                "Revisa los archivos que realmente EXISTEN en el repo HF. "
                "Sube solamente checkpoints nuevos y validados con su nombre "
                "original junto a config.json directamente en la raíz. "
                "**Nunca elimina ni sobrescribe versiones anteriores en HF.** "
                "Un registro local no prueba que el archivo remoto exista. "
                "Puedes usar HF_TOKEN como secreto del entorno."
            )
            hf_repo_id = gr.Textbox(label="Repositorio HF (nombre o usuario/nombre)", placeholder="mi-voz-piper o usuario/mi-voz-piper")
            hf_token = gr.Textbox(label="HF token", type="password")
            hf_private = gr.Checkbox(value=True, label="Repo privado")
            hf_versions_btn.click(
                inspect_hf_resume,
                inputs=[resume_hf_repo, hf_repo_id, hf_token],
                outputs=[resume_hf_checkpoint, hf_versions_report],
            )
            # Fixed root-level HF layout: .ckpt files + config.json, never folders.
            hf_remote_prefix = gr.State("")
            hf_sync_interval = gr.Number(
                value=20, precision=0, label="Buscar checkpoints nuevos cada N segundos (mín. 15)"
            )
            hf_auto_backup = gr.Checkbox(
                value=True,
                label="Respaldar automáticamente en HF cada nuevo checkpoint válido",
            )
            gr.Markdown(
                "El respaldo puede iniciarse **aunque este entrenamiento ya esté "
                "corriendo en otra instancia**. Subirá los checkpoints validados "
                "sin borrar ningún archivo del repo HF."
            )
            with gr.Row():
                enable_backup_btn = gr.Button("Activar respaldo HF independiente")
                disable_backup_btn = gr.Button("Detener solo respaldo HF")
                check_backup_btn = gr.Button("Estado del respaldo")
            backup_report = gr.Textbox(
                label="Respaldo automático HF (sobrevive al refresco de esta página)",
                lines=3, interactive=False,
            )
            enable_backup_btn.click(
                activate_backup,
                inputs=[project_name, hf_repo_id, hf_token, hf_private, hf_sync_interval],
                outputs=[backup_report],
            )
            disable_backup_btn.click(disable_backup, outputs=[backup_report])
            check_backup_btn.click(backup_status, outputs=[backup_report])

            training_notice = gr.Textbox(
                label="Estado del entrenador (actualización automática)",
                interactive=False, lines=2,
            )
            start_btn = gr.Button("Iniciar entrenamiento", variant="primary", visible=True)
            stop_btn = gr.Button("Detener entrenamiento", variant="stop", visible=False)
            start_report = gr.Textbox(label="Inicio/Stop", lines=10)
            start_btn.click(
                start_training,
                inputs=[
                    project_name,
                    mode,
                    base_checkpoint_url,
                    base_checkpoint_upload,
                    resume_checkpoint_upload,
                    resume_checkpoint_url,
                    batch_size,
                    max_epochs,
                    checkpoint_epochs,
                    checkpoint_minutes,
                    max_phoneme_ids,
                    accelerator,
                    hf_repo_id,
                    hf_token,
                    hf_private,
                    hf_remote_prefix,
                    hf_sync_interval,
                    hf_auto_backup,
                    resume_source,
                    resume_hf_repo,
                    resume_hf_checkpoint,
                ],
                outputs=start_report,
            ).then(
                training_buttons, inputs=[project_name],
                outputs=[start_btn, stop_btn, training_notice],
            )
            stop_btn.click(
                stop_training, inputs=[project_name], outputs=start_report
            ).then(
                training_buttons, inputs=[project_name],
                outputs=[start_btn, stop_btn, training_notice],
            )

        with gr.Tab("4. Estado y checkpoints"):
            refresh_btn = gr.Button("Actualizar estado")
            status_box = gr.Textbox(label="Estado/log", lines=24)
            checkpoint_files = gr.File(
                label="Archivos de recuperación: config.json + checkpoints",
                file_count="multiple",
                interactive=False,
            )
            refresh_btn.click(
                refresh_status,
                inputs=[project_name],
                outputs=[status_box, checkpoint_files],
            )
            sync_btn = gr.Button("Respaldar checkpoints nuevos en Hugging Face", variant="primary")
            sync_report = gr.Textbox(label="Resultado del respaldo incremental HF", lines=9)
            sync_btn.click(
                sync_to_huggingface,
                inputs=[project_name, hf_repo_id, hf_token, hf_private, hf_remote_prefix],
                outputs=sync_report,
            )

        with gr.Tab("5. Inferencia y exportación ONNX"):
            gr.Markdown(
                "Prueba los checkpoints sin detener el entrenamiento. "
                "La inferencia usa CPU para no ocupar VRAM mientras entrenas. "
                "**Solo abre checkpoints de confianza:** el formato Lightning usa pickle."
            )
            refresh_infer = gr.Button("Buscar checkpoints del proyecto")
            inference_checkpoint = gr.Dropdown(
                choices=[], label="Checkpoint local para escuchar",
                allow_custom_value=False,
            )
            refresh_infer.click(
                inference_choices, inputs=[project_name], outputs=[inference_checkpoint]
            )
            infer_uploaded = gr.File(
                label="O sube un checkpoint .ckpt", file_types=[".ckpt"],
                type="filepath",
            )
            infer_config_uploaded = gr.File(
                label="O sube config.json (opcional si el proyecto ya tiene uno)",
                file_types=[".json"], type="filepath",
            )
            infer_text = gr.Textbox(
                label="Texto para pronunciar", lines=4,
                value="Hola, esta es una prueba de voz en español de México.",
                max_lines=8,
            )
            infer_language = gr.Textbox(value="es-419", label="Idioma de fonemización")
            with gr.Row():
                length_scale = gr.Slider(0.5, 2.0, value=1.0, step=0.05, label="Duración / velocidad")
                noise_scale = gr.Slider(0.1, 1.0, value=0.667, step=0.01, label="Variabilidad")
                noise_w = gr.Slider(0.1, 1.0, value=0.8, step=0.01, label="Variabilidad duración")
            btn_infer = gr.Button("Generar y escuchar voz", variant="primary")
            infer_audio = gr.Audio(label="Audio generado", type="filepath", interactive=False)
            infer_wav_file = gr.File(label="Descargar WAV", interactive=False)
            infer_report = gr.Textbox(label="Resultado", lines=5)
            btn_infer.click(
                gradio_synthesize,
                inputs=[
                    project_name, inference_checkpoint, infer_uploaded,
                    infer_config_uploaded, infer_text, infer_language,
                    length_scale, noise_scale, noise_w,
                ],
                outputs=[infer_audio, infer_wav_file, infer_report],
            )
            gr.Markdown("### Exportar a Piper ONNX")
            export_btn = gr.Button("Exportar ONNX + config y comprobar inferencia")
            export_files = gr.File(
                label="Descargas ONNX y config.json", file_count="multiple",
                interactive=False,
            )
            export_report = gr.Textbox(label="Exportación", lines=6)
            export_btn.click(
                gradio_export_onnx,
                inputs=[
                    project_name, inference_checkpoint,
                    infer_uploaded, infer_config_uploaded,
                ],
                outputs=[export_files, export_report],
            )

        gr.Markdown(
            "### Recuperación\n"
            "- Si Kaggle reinicia, vuelve a lanzar Gradio y usa Resume de corrida con "
            "el último .ckpt local o uno descargado desde Hugging Face.\n"
            "- Fine-tune solo carga pesos del modelo base y comienza en epoch 0.\n"
            "- Resume restaura epoch/global step/optimizadores de tu propia corrida."
        )

        # Client-side refresh does not reset the trainer: settings and completed
        # artifacts are restored from disk, not gr.State or browser storage.
        from .studio_state import FIELDS
        persisted_controls = {
            "source_url": source_url,
            "language": language,
            "sample_rate": sample_rate,
            "max_workers": max_workers,
            "mode": mode,
            "base_checkpoint_url": base_checkpoint_url,
            "resume_checkpoint_url": resume_checkpoint_url,
            "resume_source": resume_source,
            "resume_hf_repo": resume_hf_repo,
            "resume_hf_checkpoint": resume_hf_checkpoint,
            "batch_size": batch_size,
            "max_epochs": max_epochs,
            "checkpoint_epochs": checkpoint_epochs,
            "checkpoint_minutes": checkpoint_minutes,
            "max_phoneme_ids": max_phoneme_ids,
            "accelerator": accelerator,
            "hf_repo_id": hf_repo_id,
            "hf_private": hf_private,
            "hf_sync_interval": hf_sync_interval,
            "hf_auto_backup": hf_auto_backup,
            "inference_text": infer_text,
            "inference_language": infer_language,
            "length_scale": length_scale,
            "noise_scale": noise_scale,
            "noise_w": noise_w,
        }
        controls = [persisted_controls[field] for field in FIELDS]
        assert len(controls) == len(FIELDS)
        outputs = controls + [
            status_box, checkpoint_files, inference_checkpoint,
            preferences_status, backup_report,
            start_btn, stop_btn, training_notice,
        ]
        demo.load(
            lambda: load_ui_preferences(include_name=True),
            inputs=None, outputs=[project_name] + outputs,
            show_progress="hidden",
        )
        project_name.input(
            lambda name: load_ui_preferences(name, include_name=False),
            inputs=[project_name], outputs=outputs,
            show_progress="hidden",
        )
        # Poll the actual kernel process table, so controls remain correct
        # when a different Gradio instance owns the training job.
        pulse = gr.Timer(7)
        pulse.tick(
            training_buttons, inputs=[project_name],
            outputs=[start_btn, stop_btn, training_notice],
            show_progress="hidden",
        )
        for component in controls:
            component.input(
                save_ui_preferences,
                inputs=[project_name] + controls,
                outputs=[preferences_status],
                show_progress="hidden",
            )

    return demo


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--share", action="store_true", help="Crear URL pública temporal de Gradio")
    parser.add_argument("--port", type=int, default=7860)
    args = parser.parse_args()

    KAGGLE_ROOT.mkdir(parents=True, exist_ok=True)
    demo = build_ui()
    auth_user = os.environ.get("PIPER_STUDIO_AUTH_USER", "").strip()
    auth_password = os.environ.get("PIPER_STUDIO_AUTH_PASSWORD", "")
    auth = (auth_user, auth_password) if auth_user and auth_password else None
    if args.share and auth is None:
        print(
            "WARNING: Public Gradio link has no authentication; "
            "set PIPER_STUDIO_AUTH_USER and PIPER_STUDIO_AUTH_PASSWORD.",
            flush=True,
        )
    demo.queue(default_concurrency_limit=2).launch(
        auth=auth,
        server_name="0.0.0.0",
        server_port=args.port,
        share=args.share,
        show_error=True,
        allowed_paths=[str(KAGGLE_ROOT)],
        max_file_size="10gb",
    )


if __name__ == "__main__":
    main()
