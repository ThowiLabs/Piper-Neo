"""Atomic, validated Hugging Face backup of Piper Lightning checkpoints.

The user-facing Gradio adapter supplies a Hugging Face token. This module
does not expose that token to the checkpoint validator subprocess.
"""
from __future__ import annotations

import json
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path
from typing import Any, Optional

from huggingface_hub import HfApi

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


def _reject_existing_remote_checkpoint(
    api: Any, repo_id: str, remote_path: str, sha256: str
) -> bool:
    """Prevent overwriting ANY previously published checkpoint on HF.

    Return True only if a remote checkpoint has an independently verifiable
    matching LFS SHA-256. Without an accessible SHA, fail closed.
    """
    try:
        infos = api.get_paths_info(
            repo_id, [remote_path], repo_type="model", expand=True
        )
    except TypeError:
        infos = api.get_paths_info(repo_id, [remote_path], repo_type="model")
    if not infos:
        return False
    if len(infos) != 1 or getattr(infos[0], "path", None) != remote_path:
        raise RuntimeError("HF returned ambiguous checkpoint paths")
    lfs = getattr(infos[0], "lfs", None)
    stored_sha = (
        lfs.get("sha256")
        if isinstance(lfs, dict)
        else getattr(lfs, "sha256", None)
    )
    if stored_sha and stored_sha.lower() == sha256.lower():
        return True
    raise ValueError(
        f"El checkpoint {remote_path} ya existe en HF y no puede verificarse "
        "que sea idéntico. No se sobrescribe ninguna versión."
    )


def _confirmed_remote_size(api: Any, repo_id: str, remote_path: str, expected: int) -> None:
    infos = api.get_paths_info(repo_id, [remote_path], repo_type="model")
    if len(infos) != 1 or getattr(infos[0], "path", None) != remote_path:
        raise RuntimeError("HF upload not visible at expected remote path")
    size = getattr(infos[0], "size", None)
    if size != expected:
        raise RuntimeError(f"HF file size differs ({size} != {expected})")


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
    if prefix and (any(part in {".", "..", ""} for part in prefix.split("/"))
                   or prefix.startswith("/")):
        raise ValueError("Invalid remote folder path")
    config = project / "training" / "config.json"
    if not config.is_file():
        raise FileNotFoundError(f"Missing config.json: {config}")
    with config.open(encoding="utf-8") as stream:
        cfg = json.load(stream)
    if cfg.get("num_speakers", 0) < 1 or cfg.get("audio", {}).get("sample_rate", 0) < 8000:
        raise ValueError("config.json is invalid")

    candidates = checkpoint_candidates(project)
    result = {"uploaded": [], "skipped": [], "failed": [], "repo_id": repo_id}
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
            known = manifest.get(key)
            if known and known.get("signature") == list(before):
                result["skipped"].append(checkpoint.name + " (already uploaded)")
                continue

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
                remote_config = f"{prefix}/config.json" if prefix else "config.json"
                api.upload_file(
                    path_or_fileobj=str(config), path_in_repo=remote_config,
                    repo_id=repo_id, repo_type="model",
                    commit_message="Update validated Piper configuration",
                )
                _confirmed_remote_size(api, repo_id, remote_config, config.stat().st_size)

                already_on_hf = _reject_existing_remote_checkpoint(
                    api, repo_id, remote_path, report["sha256"]
                )
                if not already_on_hf:
                    api.upload_file(
                        path_or_fileobj=str(snapshot), path_in_repo=remote_path,
                        repo_id=repo_id, repo_type="model",
                        commit_message=f"Verified Piper {checkpoint.name} step {report['global_step']}",
                    )
                _confirmed_remote_size(api, repo_id, remote_path, before[0])
                manifest[key] = {
                    "signature": list(before),
                    "sha256": report["sha256"],
                    "epoch": report["epoch"],
                    "step": report["global_step"],
                    "validated": True,
                }
                _write_manifest(project, manifest)
                result["uploaded"].append(checkpoint.name)
            finally:
                if snapshot.exists():
                    snapshot.unlink()
        except (OSError, ValueError, RuntimeError, subprocess.TimeoutExpired) as error:
            result["failed"].append({"file": checkpoint.name, "reason": str(error)})
    return result
