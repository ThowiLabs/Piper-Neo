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
import tempfile
import time
from pathlib import Path
from typing import Any, Optional

from huggingface_hub import HfApi
from huggingface_hub.utils import HfHubHTTPError, RepositoryNotFoundError

from .hf_settings import (
    DEFAULT_HF_REPO, hf_auth_token, select_hf_repo,
    require_hf_auth_for_private_repo,
)

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


def _ensure_remote_model_repo(api: Any, repo_id: str, token: str, private: bool) -> str:
    """Never call /api/repos/create for an existing private model repo.

    Repo creation needs extra permissions and can fail with 401 even when
    uploading to an *existing* repo is allowed by a fine-grained token.
    """
    try:
        api.repo_info(repo_id=repo_id, repo_type="model", token=token)
        return "already_exists"
    except RepositoryNotFoundError as error:
        code = getattr(getattr(error, "response", None), "status_code", None)
        # HF can disguise inaccessible private models as Not Found. This
        # known existing repo must NEVER be treated as a new empty repo.
        if code in (401, 403) or repo_id == DEFAULT_HF_REPO:
            raise PermissionError(
                f"No se puede acceder al repositorio privado {repo_id} "
                f"(HTTP {code or 'desconocido'}). Verifica que el token tenga "
                "permiso de lectura y escritura sobre ESE repositorio; "
                "no se intentará recrearlo."
            ) from error
        if code != 404:
            raise
        # Only a genuinely new repo (not the known private backup) can be
        # created after an authenticated, exact 404 response.
    except HfHubHTTPError as error:
        status = getattr(getattr(error, "response", None), "status_code", None)
        if status in (401, 403):
            raise PermissionError(
                f"HF devolvió HTTP {status} al acceder a {repo_id}. "
                "El token no puede leer este repositorio privado, está "
                "caducado o no tiene permisos para este repo."
            ) from error
        raise
    api.create_repo(
        repo_id=repo_id, repo_type="model", token=token,
        private=bool(private), exist_ok=True,
    )
    return "created"


def _cli_upload_checkpoint(repo_id: str, checkpoint: Path, token: str) -> None:
    """Use the proven CLI transport if HfApi's large-file transfer fails.

    The token is passed ONLY by process environment, never on command line.
    This helper must be called only after the Hub was checked for an existing
    remote filename. Never use a delete/overwrite or force flag.
    """
    cli = Path(sys.executable).with_name("huggingface-cli")
    if not cli.is_file():
        raise RuntimeError(
            "No existe huggingface-cli en el entorno Python; "
            "comprueba la instalación de huggingface_hub."
        )
    env = {**os.environ, "HF_TOKEN": token, "HF_HUB_ENABLE_HF_TRANSFER": "0"}
    result = subprocess.run(
        [str(cli), "upload", repo_id, str(checkpoint), checkpoint.name,
         "--repo-type", "model",
         "--commit-message", f"Back up validated Piper {checkpoint.name}"],
        env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        text=True, timeout=900,
    )
    if result.returncode:
        raise RuntimeError(
            f"La subida alternativa CLI falló (código {result.returncode}): "
            + (result.stderr or result.stdout or "sin detalles")[-1200:]
        )


def _ensure_remote_config(api: Any, repo_id: str, token: str, config: Path) -> str:
    """Only upload config.json if absent; an existing remote is NEVER compared.

    A new CKPT is validated with the LOCAL training/config.json before upload.
    The shared remote config.json may legitimately differ in size/content after
    reprocessing the dataset; such differences do not block CKPT backups.
    Resume still enforces compatibility independently in hf_resume.py.
    """
    remote_path = "config.json"
    if _remote_info(api, repo_id, remote_path) is not None:
        return "already_remote"
    api.upload_file(
        path_or_fileobj=str(config), path_in_repo=remote_path,
        repo_id=repo_id, repo_type="model", token=token,
        commit_message="Add Piper config.json (append-only backup)",
    )
    _confirmed_remote_size(api, repo_id, remote_path, config.stat().st_size)
    return "uploaded"


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
    repo_id = select_hf_repo(repo_id)
    token = hf_auth_token(token)
    require_hf_auth_for_private_repo(repo_id, token)
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
    # Different Gradio processes can watch the same project simultaneously.
    # Never share a snapshot path between processes or backup requests.
    unique_stage_dir = Path(tempfile.mkdtemp(prefix="hf-backup-", dir=stage_dir))
    remote_repo_checked = False

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

            snapshot = unique_stage_dir / checkpoint.name
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
                if not remote_repo_checked:
                    _ensure_remote_model_repo(api, repo_id, token, bool(private))
                    remote_repo_checked = True
                config_status = _ensure_remote_config(api, repo_id, token, config)
                result["config_status"] = config_status

                remote_status = _remote_checkpoint_status(
                    api, repo_id, remote_path, before[0], report["sha256"]
                )
                if remote_status == "missing":
                    try:
                        api.upload_file(
                            path_or_fileobj=str(snapshot), path_in_repo=remote_path,
                            repo_id=repo_id, repo_type="model", token=token,
                            commit_message=f"Add verified Piper {checkpoint.name} step {report['global_step']}",
                        )
                    except Exception as upload_error:
                        status_code = getattr(
                            getattr(upload_error, "response", None), "status_code", None
                        )
                        # Bad permissions must be fixed by the owner; an
                        # alternate transport cannot repair a 401/403.
                        if status_code in (401, 403):
                            raise PermissionError(
                                f"HF rechazó subida de {checkpoint.name}: HTTP {status_code}. "
                                "Revisa el token y permisos Write sobre el repo privado."
                            ) from upload_error
                        # The API might have committed despite losing the HTTP
                        # response. Query the Hub AGAIN to avoid overwriting.
                        retry_status = _remote_checkpoint_status(
                            api, repo_id, remote_path, before[0], report["sha256"]
                        )
                        if retry_status == "missing":
                            try:
                                _cli_upload_checkpoint(repo_id, snapshot, token)
                            except Exception as cli_error:
                                raise RuntimeError(
                                    f"Falló API para {checkpoint.name}: "
                                    f"{type(upload_error).__name__}: {upload_error}. "
                                    f"También falló CLI: {cli_error}"
                                ) from cli_error
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
    # Only our request's isolated staging directory can be removed.
    shutil.rmtree(unique_stage_dir, ignore_errors=True)
    return result
