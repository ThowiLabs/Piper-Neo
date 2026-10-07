"""Atomic, validated Hugging Face backup of Piper Lightning checkpoints.

The user-facing Gradio adapter supplies a Hugging Face token. This module
does not expose that token to the checkpoint validator subprocess.
"""
from __future__ import annotations

import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path
from typing import Any, Optional

from huggingface_hub import HfApi, hf_hub_download

from .checkpoint_guard import CHECKPOINT_NAME, stable_file


def resolve_repo_id(repo_name: str, token: str, api: Optional[Any] = None) -> str:
    """Accept namespace/model or a short model name owned by the HF token."""
    name = (repo_name or "").strip()
    if not name:
        raise ValueError("Indica el nombre del repositorio HF.")
    parts = name.split("/")
    if len(parts) not in (1, 2) or not all(
        re.fullmatch(r"[\w.-]+", part) and part not in {".", ".."}
        for part in parts
    ):
        raise ValueError("Repo HF inválido: usa modelo o usuario/modelo.")
    if len(parts) == 2:
        return name
    hub = api or HfApi(token=token)
    profile = hub.whoami(token=token)
    username = profile.get("name") or profile.get("fullname")
    if not username:
        raise ValueError("No se pudo identificar el usuario del token HF.")
    return f"{username}/{name}"


def checkpoint_candidates(project: Path):
    training = project / "training"
    candidates = training.rglob("epoch=*-step=*.ckpt")
    return sorted(
        (p for p in candidates if CHECKPOINT_NAME.fullmatch(p.name)),
        key=lambda p: (
            int(CHECKPOINT_NAME.fullmatch(p.name)[1]),
            int(CHECKPOINT_NAME.fullmatch(p.name)[2]),
            p.name,
        ),
    )


def _validate_in_subprocess(snapshot: Path, config: Path, timeout: int = 180) -> dict:
    """Isolate pickle loading from the main Gradio process and HF token."""
    env = {
        k: v for k, v in os.environ.items()
        if not k.upper().startswith(("HF_", "HUGGINGFACE_", "HUGGING_FACE_", "GITHUB_TOKEN"))
    }
    result = subprocess.run(
        [sys.executable, "-m", "piper_train.checkpoint_guard",
         str(snapshot), str(config), "--min-age", "0"],
        capture_output=True, text=True, timeout=timeout, env=env,
    )
    line = (result.stdout or "").strip().splitlines()
    payload = json.loads(line[-1]) if line else {}
    if result.returncode or payload.get("status") != "structurally_valid":
        raise ValueError(payload.get("error", result.stderr[-1000:] or "Failed validation"))
    return payload


def _manifest_path(project: Path) -> Path:
    return project / ".hf_synced_verified.json"


def _read_manifest(project: Path) -> dict:
    path = _manifest_path(project)
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return {}


def _write_manifest(project: Path, manifest: dict) -> None:
    path = _manifest_path(project)
    temp = path.with_suffix(".json.tmp")
    temp.write_text(json.dumps(manifest, indent=2, ensure_ascii=False), encoding="utf-8")
    os.replace(temp, path)


def _remote_info(api: Any, repo_id: str, remote_path: str):
    """Query Hugging Face EVERY time; a local history file is never proof."""
    try:
        infos = api.get_paths_info(
            repo_id, [remote_path], repo_type="model", expand=True
        )
    except TypeError:
        infos = api.get_paths_info(repo_id, [remote_path], repo_type="model")
    if not infos:
        return None
    if len(infos) != 1 or getattr(infos[0], "path", None) != remote_path:
        raise RuntimeError(f"HF devolvió una ruta ambigua para {remote_path}")
    return infos[0]


def _remote_sha(info) -> str | None:
    lfs = getattr(info, "lfs", None)
    return (
        lfs.get("sha256") if isinstance(lfs, dict)
        else getattr(lfs, "sha256", None)
    )


def _remote_checkpoint_status(
    api: Any, repo_id: str, remote_path: str, local_size: int, local_sha: str
) -> str:
    """Return missing, verified, or present_without_sha. NEVER overwrite."""
    info = _remote_info(api, repo_id, remote_path)
    if info is None:
        return "missing"
    size = getattr(info, "size", None)
    if size != local_size:
        raise ValueError(
            f"{remote_path} YA EXISTE en HF con tamaño diferente "
            f"({size} vs {local_size}). Se conserva el remoto sin sobrescribirlo."
        )
    remote_sha = _remote_sha(info)
    if remote_sha and remote_sha.lower() != local_sha.lower():
        raise ValueError(
            f"{remote_path} YA EXISTE en HF con SHA256 diferente. "
            "Se conserva el remoto sin sobrescribirlo."
        )
    return "verified" if remote_sha else "present_without_sha"


def _confirmed_remote_size(api: Any, repo_id: str, remote_path: str, expected: int) -> None:
    # HF's metadata may take a moment to become visible after a commit.
    for attempt in range(3):
        info = _remote_info(api, repo_id, remote_path)
        if info is not None and getattr(info, "size", None) == expected:
            return
        if attempt < 2:
            time.sleep(1)
    raise RuntimeError(
        f"HF no confirmó {remote_path} o el tamaño no coincide ({expected}). "
        "Se reintentará en el próximo respaldo; no se marcará como subido."
    )


def _sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _ensure_remote_config(api: Any, repo_id: str, token: str, config: Path) -> str:
    """Upload missing config.json or confirm identical bytes. Never replace."""
    remote_path = "config.json"
    info = _remote_info(api, repo_id, remote_path)
    if info is None:
        # Checkpoint is already valid before this function is called.
        api.upload_file(
            path_or_fileobj=str(config), path_in_repo=remote_path,
            repo_id=repo_id, repo_type="model",
            commit_message="Add Piper config.json (append-only backup)",
        )
        _confirmed_remote_size(api, repo_id, remote_path, config.stat().st_size)
        return "uploaded"
    size = getattr(info, "size", None)
    if size != config.stat().st_size:
        raise ValueError(
            "config.json YA EXISTE en HF pero su tamaño es diferente. "
            "No se sobrescribe: revisa que sea la misma voz/dataset."
        )
    # Small config: verify content, not only size. In tests the fake API exposes
    # a download helper; in production Hugging Face provides hf_hub_download.
    if hasattr(api, "download_file"):
        remote_file = Path(api.download_file(repo_id=repo_id, filename=remote_path))
    else:
        remote_file = Path(hf_hub_download(
            repo_id=repo_id, filename=remote_path, repo_type="model",
            token=token or None,
        ))
    if _sha256_file(remote_file) != _sha256_file(config):
        raise ValueError(
            "config.json YA EXISTE en HF pero su contenido es diferente. "
            "Se conserva la versión remota sin cambios."
        )
    return "already_remote"


def sync_verified_checkpoints(
    project: Path,
    repo_id: str,
    token: str,
    private: bool = True,
    remote_prefix: str = "",
    *,
    api: Optional[Any] = None,
    validator=None,
    min_age: float = 12.0,
) -> dict:
    """Only upload complete verified checkpoints and config.json.

    Does not upload `last.ckpt` (duplicate, huge, and overwritten by Lightning).
    Preserves exact Lightning filename and uses the repo root by default.
    Validate from a temporary immutable COPY, not the file being written.
    Upload commits are confirmed with HF path and size before marking synced.
    """
    project = Path(project)
    repo_id = (repo_id or "").strip()
    if not repo_id:
        raise ValueError("HF repo ID is required")
    # This also validates explicit namespace/model names. For short names the
    # authenticated account determines the namespace without user guesswork.
    api = api or HfApi(token=token)
    repo_id = resolve_repo_id(repo_id, token, api=api)
    prefix = (remote_prefix or "").strip("/")
    if prefix:
        raise ValueError(
            "El respaldo de Piper no permite carpetas en HF: "
            "config.json y cada checkpoint deben quedar en la raíz."
        )
    config = project / "training" / "config.json"
    if not config.is_file():
        raise FileNotFoundError(f"Missing config.json: {config}")
    with config.open(encoding="utf-8") as stream:
        cfg = json.load(stream)
    if cfg.get("num_speakers", 0) < 1 or cfg.get("audio", {}).get("sample_rate", 0) < 8000:
        raise ValueError("config.json is invalid")

    candidates = checkpoint_candidates(project)
    result = {
        "uploaded": [], "skipped": [], "failed": [],
        "repo_id": repo_id, "config_status": "pending",
    }
    if not candidates:
        return result

    manifest = _read_manifest(project)
    verify = validator or _validate_in_subprocess
    stage_dir = project / ".hf_staging"
    stage_dir.mkdir(parents=True, exist_ok=True)

    for checkpoint in candidates:
        remote_path = f"{prefix}/{checkpoint.name}" if prefix else checkpoint.name
        key = f"{repo_id}::{remote_path}"
        try:
            before = stable_file(checkpoint, min_age=min_age)
            # Local .hf_synced_verified.json is audit history ONLY. A previous
            # upload is not considered present until HF itself confirms it.
            # This fixes repos that were deleted/recreated after first backup.

            # Enough free space for one additional checkpoint and some headroom.
            if shutil.disk_usage(stage_dir).free < before[0] + 512 * 1024 * 1024:
                raise OSError("Insufficient free disk for safe checkpoint snapshot")

            snapshot = stage_dir / checkpoint.name
            try:
                shutil.copy2(checkpoint, snapshot)
                if stable_file(checkpoint, min_age=min_age) != before:
                    raise ValueError("Source checkpoint changed during snapshot")
                if snapshot.stat().st_size != before[0]:
                    raise ValueError("Checkpoint snapshot is incomplete")

                report = verify(snapshot, config)
                if report["size"] != before[0] or report["name"] != checkpoint.name:
                    raise ValueError("Checkpoint validator reported mismatched metadata")

                # Create the repository only after validation passes, so broken
                # checkpoints cannot trigger publishing an invalid config.
                api.create_repo(
                    repo_id=repo_id, repo_type="model", private=bool(private),
                    exist_ok=True,
                )
                config_status = _ensure_remote_config(api, repo_id, token, config)
                result["config_status"] = config_status

                remote_status = _remote_checkpoint_status(
                    api, repo_id, remote_path, before[0], report["sha256"]
                )
                if remote_status == "missing":
                    api.upload_file(
                        path_or_fileobj=str(snapshot), path_in_repo=remote_path,
                        repo_id=repo_id, repo_type="model",
                        commit_message=f"Add verified Piper {checkpoint.name} step {report['global_step']}",
                    )
                    _confirmed_remote_size(api, repo_id, remote_path, before[0])
                    remote_status = _remote_checkpoint_status(
                        api, repo_id, remote_path, before[0], report["sha256"]
                    )
                    if remote_status == "missing":
                        raise RuntimeError("HF no devolvió el checkpoint recién subido")
                    result["uploaded"].append(checkpoint.name)
                else:
                    note = (
                        "verificado remotamente"
                        if remote_status == "verified"
                        else "existe remotamente con tamaño correcto; hash LFS no disponible"
                    )
                    result["skipped"].append(f"{checkpoint.name} ({note})")

                # The manifest records proof only AFTER remote confirmation.
                manifest[key] = {
                    "signature": list(before),
                    "sha256": report["sha256"],
                    "epoch": report["epoch"],
                    "step": report["global_step"],
                    "validated": True,
                    "remote_status": remote_status,
                }
                _write_manifest(project, manifest)
            finally:
                if snapshot.exists():
                    snapshot.unlink()
        except (OSError, ValueError, RuntimeError, subprocess.TimeoutExpired) as error:
            result["failed"].append({"file": checkpoint.name, "reason": str(error)})
    return result
