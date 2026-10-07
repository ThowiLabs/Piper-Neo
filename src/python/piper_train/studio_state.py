"""Atomic on-disk Gradio preferences; never store Hugging Face secrets.

Settings and processed datasets survive browser reloads and UI process restarts.
Selected uploaded files themselves do not: use the durable project artifacts.
"""
from __future__ import annotations

import json
import os
import tempfile
import threading
from pathlib import Path

LOCK = threading.RLock()
DEFAULTS = {
    "source_url": "",
    "language": "es-419",
    "sample_rate": 22050,
    "max_workers": 4,
    "mode": "Fine-tune desde modelo base",
    "base_checkpoint_url": (
        "https://huggingface.co/datasets/rhasspy/piper-checkpoints/resolve/main/"
        "es/es_ES/davefx/medium/epoch%3D5629-step%3D1605020.ckpt"
    ),
    "resume_checkpoint_url": "",
    "resume_source": "Local: último checkpoint",
    "resume_hf_repo": "",
    "resume_hf_checkpoint": "Automático: último checkpoint válido",
    "batch_size": 16,
    "max_epochs": 1000,
    "checkpoint_epochs": 5,
    "checkpoint_minutes": 15,
    "max_phoneme_ids": 400,
    "accelerator": "auto",
    "hf_repo_id": "",
    "hf_private": True,
    "hf_sync_interval": 20,
    "hf_auto_backup": True,
    "inference_text": "Hola, esta es una prueba de voz en español de México.",
    "inference_language": "es-419",
    "length_scale": 1.0,
    "noise_scale": .667,
    "noise_w": .8,
}
# No hf_token, no destructive reset flag or uploaded temp path.
FIELDS = tuple(DEFAULTS)


def _atomic_write(path: Path, obj: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(
        "w", encoding="utf-8", dir=str(path.parent), delete=False
    ) as stream:
        json.dump(obj, stream, indent=2, ensure_ascii=False)
        temp = Path(stream.name)
    os.replace(temp, path)


def _settings_file(project: Path) -> Path:
    return project / ".studio_preferences.json"


def _last_file(root: Path) -> Path:
    return root / ".studio_last_project.json"


def save_preferences(root: Path, project_name: str, values: dict) -> dict:
    root = Path(root)
    name = project_name.strip() if project_name else "capibara"
    if not name or name.startswith(".") or "/" in name or "\\" in name:
        raise ValueError("Invalid project name")
    project = root / name
    cleaned = {
        key: values[key]
        for key in FIELDS if key in values and isinstance(values[key], (str,int,float,bool))
    }
    with LOCK:
        project.mkdir(parents=True, exist_ok=True)
        existing = load_preferences(root, name)[1]
        existing.update(cleaned)
        _atomic_write(_settings_file(project), existing)
        _atomic_write(_last_file(root), {"name": name})
    return existing


def load_preferences(root: Path, name: str | None = None) -> tuple[str, dict]:
    root = Path(root)
    with LOCK:
        if not name:
            try:
                name = json.loads(_last_file(root).read_text(encoding="utf-8")).get("name")
            except (OSError, ValueError):
                name = "capibara"
        if not name or name.startswith(".") or "/" in name or "\\" in name:
            name = "capibara"
        values = DEFAULTS.copy()
        try:
            saved = json.loads(_settings_file(root / name).read_text(encoding="utf-8"))
            for key in FIELDS:
                if key in saved and isinstance(saved[key], type(DEFAULTS[key])):
                    values[key] = saved[key]
        except (OSError, ValueError, TypeError):
            pass
        return name, values
