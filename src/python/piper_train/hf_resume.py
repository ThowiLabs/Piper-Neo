"""Recover full Piper Lightning training state from the newest valid HF checkpoint.

Remote files are expected at the MODEL repository root:
    config.json, epoch=N-step=M.ckpt, ...

Security: Piper Lightning checkpoints are Python pickle. Only resume checkpoints
from repositories you trust; the validator runs in a separate process but is not
a sandbox. The HF token is never written into preferences or metadata.
"""
from __future__ import annotations

import hashlib
import json
import os
import re
import subprocess
import sys
from pathlib import Path
from typing import Any, Callable, Optional

from huggingface_hub import HfApi, hf_hub_download

from .checkpoint_guard import CHECKPOINT_NAME
from .hf_backup import resolve_repo_id

ROOT_SELECTION = "Automático: último checkpoint válido"


def _rank_checkpoint(path: str) -> tuple[int, int, int]:
    match = CHECKPOINT_NAME.fullmatch(path)
    if match is None or "/" in path:
        raise ValueError("No es checkpoint de Piper en raíz de HF")
    v = re.search(r"-v(\d+)\.ckpt$", path)
    return int(match.group(2)), int(match.group(1)), int(v.group(1)) if v else 0


def list_remote_versions(
    repo_name: str, token: str = "", *, api: Optional[Any] = None
) -> tuple[str, list[str]]:
    """Return newest first, using *global_step*, not lexicographic order."""
    hub = api or HfApi(token=token or None)
    repo_id = resolve_repo_id(repo_name, token, api=hub)
    files = hub.list_repo_files(repo_id=repo_id, repo_type="model")
    if "config.json" not in files:
        raise ValueError(f"{repo_id} no tiene config.json en la raíz")
    versions = [p for p in files if "/" not in p and CHECKPOINT_NAME.fullmatch(p)]
    versions.sort(key=_rank_checkpoint, reverse=True)
    if not versions:
        raise ValueError(f"{repo_id} no tiene checkpoints epoch=N-step=N.ckpt en la raíz")
    return repo_id, versions


def _hf_download(
    repo_id: str, filename: str, target_dir: Path, token: str,
) -> Path:
    # local_dir stores the file directly under target_dir without duplicating
    # the full 800MB object in the global HF hub cache.
    return Path(hf_hub_download(
        repo_id=repo_id, filename=filename, repo_type="model",
        local_dir=str(target_dir), token=token or None,
    ))


def _digest(path: Path) -> str:
    sha = hashlib.sha256()
    with path.open("rb") as src:
        while part := src.read(8 * 1024 * 1024):
            sha.update(part)
    return sha.hexdigest()


def _assert_remote_integrity(
    hub: Any, repo_id: str, filename: str, path: Path
) -> None:
    # LFS hashes are independent of filename. Fail closed on mismatches.
    try:
        metadata = hub.get_paths_info(
            repo_id, [filename], repo_type="model", expand=True
        )
    except TypeError:
        metadata = hub.get_paths_info(repo_id, [filename], repo_type="model")
    if len(metadata) != 1 or getattr(metadata[0], "path", None) != filename:
        raise ValueError(f"No se pudo comprobar la existencia remota: {filename}")
    size = getattr(metadata[0], "size", None)
    if size is not None and path.stat().st_size != size:
        raise ValueError(f"El archivo descargado {filename} tiene tamaño incorrecto")
    lfs = getattr(metadata[0], "lfs", None)
    expected_sha = lfs.get("sha256") if isinstance(lfs, dict) else getattr(lfs, "sha256", None)
    if expected_sha and _digest(path).lower() != expected_sha.lower():
        raise ValueError(f"El SHA256 de {filename} no coincide con Hugging Face")


def _compatible_config(existing: Path, remote: Path) -> None:
    with remote.open(encoding="utf-8") as src:
        remote_config = json.load(src)
    if not existing.is_file():
        raise FileNotFoundError("Falta training/config.json. Vuelve a preprocesar el dataset.")
    with existing.open(encoding="utf-8") as src:
        local_config = json.load(src)
    # Every attribute defining interpretation of the audio/phoneme IDs
    # must remain identical; audio length/sample rate & architecture matter.
    for key in ("num_symbols", "num_speakers", "phoneme_type", "phoneme_id_map"):
        if local_config.get(key) != remote_config.get(key):
            raise ValueError(f"Config local e HF incompatibles: {key}")
    for key in ("sample_rate",):
        if local_config.get("audio", {}).get(key) != remote_config.get("audio", {}).get(key):
            raise ValueError(f"Config local e HF incompatibles: audio.{key}")
    local_voice = local_config.get("espeak", {}).get("voice")
    remote_voice = remote_config.get("espeak", {}).get("voice")
    if local_voice != remote_voice:
        raise ValueError("Idioma/voz eSpeak del dataset difiere de HF")


def _validate_trusted_checkpoint(ckpt: Path, config: Path) -> dict:
    # Intentionally isolated from the Gradio server; no HF token forwarded.
    env = {
        k: v for k, v in os.environ.items()
        if not k.upper().startswith(("HF_", "HUGGINGFACE_", "HUGGING_FACE_", "GITHUB_TOKEN"))
    }
    process = subprocess.run(
        [sys.executable, "-m", "piper_train.checkpoint_guard", str(ckpt),
         str(config), "--min-age", "0"],
        capture_output=True, text=True, env=env, timeout=180,
    )
    output = (process.stdout or "").strip().splitlines()
    result = json.loads(output[-1]) if output else {}
    if process.returncode != 0 or result.get("status") != "structurally_valid":
        raise ValueError(result.get("error", (process.stderr or "")[-500:] or "CKPT inválido"))
    return result


def prepare_resume_from_hf(
    project: Path,
    repo_name: str,
    token: str = "",
    selected_checkpoint: str = ROOT_SELECTION,
    *,
    max_epochs: Optional[int] = None,
    api: Optional[Any] = None,
    download: Optional[Callable] = None,
    validator: Optional[Callable] = None,
) -> dict:
    """Download and verify the newest valid checkpoint, preserving the dataset.

    If latest is corrupt, attempt earlier checkpoints (automatic selection only).
    If a specific name was selected, never silently replace that choice.
    """
    project = Path(project)
    training = project / "training"
    if not (training / "dataset.jsonl").exists():
        raise ValueError(
            "No existe dataset.jsonl local. Primero prepara y preprocesa "
            "el dataset original; HF guarda los checkpoints pero no el dataset."
        )
    local_config = training / "config.json"
    hub = api or HfApi(token=token or None)
    repo_id, names = list_remote_versions(repo_name, token, api=hub)
    automatic = selected_checkpoint in ("", None, ROOT_SELECTION)
    if not automatic:
        if selected_checkpoint not in names:
            raise ValueError(f"Checkpoint no encontrado en HF: {selected_checkpoint}")
        names = [selected_checkpoint]

    safe_id = re.sub(r"[^a-zA-Z0-9._-]", "_", repo_id)
    folder = project / "resume_hf" / safe_id
    folder.mkdir(parents=True, exist_ok=True)
    get = download or _hf_download

    remote_config = get(repo_id, "config.json", folder, token)
    _assert_remote_integrity(hub, repo_id, "config.json", remote_config)
    _compatible_config(local_config, remote_config)
    errors = []

    for filename in names:
        try:
            # Avoid a disk-full crash during the 800MB checkpoint download.
            import shutil
            if shutil.disk_usage(folder).free < 1050 * 1024 * 1024:
                raise OSError("Espacio insuficiente para descargar otro checkpoint (~1GB libre requerido)")
            checkpoint = get(repo_id, filename, folder, token)
            _assert_remote_integrity(hub, repo_id, filename, checkpoint)
            report = (validator or _validate_trusted_checkpoint)(checkpoint, remote_config)
            if (report["epoch"], report["global_step"]) != (
                int(CHECKPOINT_NAME.fullmatch(filename)[1]),
                int(CHECKPOINT_NAME.fullmatch(filename)[2]),
            ):
                raise ValueError("El checkpoint contiene epoch/step distintos de su nombre")
            if max_epochs is not None and int(max_epochs) <= report["epoch"]:
                raise ValueError(
                    f"max_epochs={max_epochs} no permite continuar después de epoch="
                    f"{report['epoch']}; aumenta las épocas antes de reanudar."
                )
            metadata = {
                "repository": repo_id,
                "filename": filename,
                "download_path": str(checkpoint),
                "epoch": report["epoch"],
                "global_step": report["global_step"],
                "sha256": report["sha256"],
                "fallback_errors": errors,
                "resume_command_option": "--resume_from_checkpoint",
            }
            # Never store the token. This metadata permits auditing and reload.
            meta_path = folder / ".last_resume.json"
            tmp = folder / ".last_resume.tmp"
            tmp.write_text(json.dumps(metadata, ensure_ascii=False, indent=2), encoding="utf-8")
            os.replace(tmp, meta_path)
            return metadata
        except (ValueError, OSError, RuntimeError, subprocess.TimeoutExpired) as error:
            errors.append(f"{filename}: {error}")
            if not automatic:
                raise ValueError(errors[-1]) from error
            # Do not delete or overwrite local training checkpoints. Remove
            # only invalid downloads under the dedicated resume_hf folder.
            candidate = folder / filename
            if candidate.is_file():
                candidate.unlink()
    raise ValueError("Ningún checkpoint HF pudo validarse: " + " | ".join(errors[:5]))
