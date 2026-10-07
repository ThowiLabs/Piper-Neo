"""Interactive text-to-speech lab for locally trained Piper checkpoints.

Inference uses CPU by default so that fine-tuning can continue on the GPU.
Only use locally generated or otherwise trusted .ckpt files: Lightning's
checkpoint deserialization uses Python pickle.
"""
from __future__ import annotations

import json
import re
import subprocess
import sys
import threading
import time
from pathlib import Path
from typing import Optional

import numpy as np
import onnxruntime as ort
import soundfile as sf
import torch
from piper_phonemize import phoneme_ids_espeak, phonemize_espeak

from .vits.lightning import VitsModel

_INFERENCE_LOCK = threading.Lock()


def find_checkpoints(project: Path) -> list[Path]:
    training = Path(project) / "training"
    return sorted(
        training.rglob("*.ckpt"),
        key=lambda p: (p.stat().st_mtime_ns, p.name),
        reverse=True,
    ) if training.exists() else []


def _input_ids(text: str, language: str) -> list[int]:
    if not text or not text.strip():
        raise ValueError("Escribe un texto antes de generar voz")
    if len(text) > 1000:
        raise ValueError("El texto no puede superar 1000 caracteres por prueba")
    phonemes = phonemize_espeak(text.strip(), language)
    flattened = [p for sentence in phonemes for p in sentence]
    ids = phoneme_ids_espeak(flattened)
    if not ids:
        raise ValueError("No se produjeron fonemas para este texto")
    return ids


def _choose_checkpoint(project: Path, checkpoint_path: str, checkpoint_upload) -> Path:
    upload_path = Path(checkpoint_upload) if checkpoint_upload else None
    if upload_path is not None:
        candidate = upload_path
    elif checkpoint_path:
        candidate = Path(checkpoint_path)
        resolved = candidate.resolve()
        training = (Path(project) / "training").resolve()
        base = (Path(project) / "base").resolve()
        if not (training in resolved.parents or base in resolved.parents):
            raise ValueError("Checkpoint selected outside project training/base")
    else:
        found = find_checkpoints(project)
        if not found:
            raise ValueError("No hay checkpoints; sube un .ckpt o entrena primero")
        candidate = found[0]

    if not candidate.is_file() or candidate.suffix != ".ckpt":
        raise ValueError("El checkpoint seleccionado no existe o no es .ckpt")
    return candidate


def _config(project: Path, uploaded_config=None) -> dict:
    config_path = Path(uploaded_config) if uploaded_config else Path(project) / "training" / "config.json"
    if not config_path.is_file():
        raise ValueError("Falta config.json; sube uno o preprocesa el dataset")
    config = json.loads(config_path.read_text(encoding="utf-8"))
    return config


def synthesize(
    project: Path,
    checkpoint_path: str,
    checkpoint_upload,
    config_upload,
    text: str,
    language: str,
    length_scale: float,
    noise_scale: float,
    noise_w: float,
) -> tuple[str, str, str]:
    """Generate WAV without modifying training state."""
    project = Path(project)
    ckpt = _choose_checkpoint(project, checkpoint_path, checkpoint_upload)
    config = _config(project, config_upload)
    phonemes = _input_ids(text, language)
    if not 0.4 <= float(length_scale) <= 2.5:
        raise ValueError("Velocidad fuera de rango")
    if not 0.0 <= float(noise_scale) <= 1.2 or not 0.0 <= float(noise_w) <= 1.2:
        raise ValueError("Noise fuera de rango")
    if len(phonemes) > 5000:
        raise ValueError("Texto demasiado largo en fonemas")

    with _INFERENCE_LOCK:
        model = VitsModel.load_from_checkpoint(
            str(ckpt), dataset=None, map_location="cpu"
        )
        model.eval()
        model_symbols = int(model.hparams.num_symbols)
        if model_symbols != int(config["num_symbols"]):
            raise ValueError("num_symbols no coincide entre checkpoint y config")
        sr = int(model.hparams.sample_rate)
        if sr != int(config["audio"]["sample_rate"]):
            raise ValueError("sample rate no coincide entre checkpoint y config")
        num_speakers = int(model.hparams.num_speakers)
        if num_speakers != int(config.get("num_speakers", 1)):
            raise ValueError("num_speakers no coincide entre checkpoint y config")

        sid = torch.LongTensor([0]) if num_speakers > 1 else None
        ids = torch.LongTensor(phonemes).unsqueeze(0)
        lengths = torch.LongTensor([len(phonemes)])
        with torch.inference_mode():
            audio = model(
                ids, lengths,
                [float(noise_scale), float(length_scale), float(noise_w)], sid=sid,
            )
            output = audio.detach().cpu().numpy().reshape(-1)
        del model, audio

    if output.size == 0 or not np.all(np.isfinite(output)):
        raise ValueError("Inferencia produjo audio vacío o no finito")
    output = np.clip(output, -1.0, 1.0)
    outdir = project / "inference"
    outdir.mkdir(parents=True, exist_ok=True)
    name = re.sub(r"[^A-Za-z0-9._-]", "_", ckpt.stem)
    output_path = outdir / f"{name}-{time.time_ns()}.wav"
    sf.write(str(output_path), output, sr, subtype="PCM_16")
    duration = len(output) / sr
    return str(output_path), str(output_path), (
        f"Generación completada: {ckpt.name}\n"
        f"Idioma: {language} · Frecuencia: {sr} Hz\n"
        f"Fonemas: {len(phonemes)} · Audio: {duration:.2f} segundos"
    )


def export_onnx(
    project: Path, checkpoint_path: str, checkpoint_upload, config_upload
) -> tuple[list[str], str]:
    """Export checkpoint to Piper .onnx plus matching .onnx.json; verify with ORT."""
    project = Path(project)
    checkpoint = _choose_checkpoint(project, checkpoint_path, checkpoint_upload)
    config = _config(project, config_upload)
    outdir = project / "exports"
    outdir.mkdir(parents=True, exist_ok=True)
    basename = re.sub(r"[^A-Za-z0-9._-]", "_", checkpoint.stem)
    model_path = outdir / f"{basename}.onnx"
    config_path = outdir / f"{basename}.onnx.json"
    cmd = [
        sys.executable, "-m", "piper_train.export_onnx",
        str(checkpoint), str(model_path),
    ]
    with _INFERENCE_LOCK:
        completed = subprocess.run(
            cmd, text=True, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, timeout=900,
        )
    if completed.returncode != 0:
        raise RuntimeError("Fallo exportación ONNX:\n" + completed.stdout[-3000:])
    config_path.write_text(
        json.dumps(config, indent=2, ensure_ascii=False), encoding="utf-8"
    )

    # Basic runtime verification: ONNX Runtime must load it and accept audio.
    session = ort.InferenceSession(
        str(model_path), providers=["CPUExecutionProvider"]
    )
    required = {x.name for x in session.get_inputs()}
    expected = {"input", "input_lengths", "scales"}
    if not expected.issubset(required):
        raise ValueError(f"ONNX no tiene inputs Piper requeridos: {required}")
    ids = np.asarray([_input_ids("Hola, prueba de voz.", config["espeak"]["voice"])], dtype=np.int64)
    feed = {
        "input": ids,
        "input_lengths": np.asarray([ids.shape[1]], dtype=np.int64),
        "scales": np.asarray([0.667, 1.0, 0.8], dtype=np.float32),
    }
    if "sid" in required:
        feed["sid"] = np.asarray([0], dtype=np.int64)
    result = session.run(None, feed)[0]
    if not isinstance(result, np.ndarray) or not result.size or not np.isfinite(result).all():
        raise ValueError("ONNX exportado no produce audio numérico válido")
    return [str(model_path), str(config_path)], (
        f"Modelo ONNX comprobado con audio de prueba:\n"
        f"{model_path.name}\n{config_path.name}"
    )
