"""Single source of truth for Piper-Neo Hugging Face configuration.

Never store HF_TOKEN in preferences, logs, code or generated project files.
"""
from __future__ import annotations

import os

DEFAULT_HF_REPO = "HirCoir/piper-checkpoint-es-mx-capybara"


def select_hf_repo(repo: str | None = None) -> str:
    """Use the user's existing checkpoint repository unless overridden."""
    return (repo or "").strip() or DEFAULT_HF_REPO


def hf_auth_token(token: str | None = None) -> str:
    """User-provided token takes precedence; Kaggle Secrets otherwise."""
    return (token or "").strip() or os.environ.get("HF_TOKEN", "").strip()


def require_hf_auth_for_private_repo(repo: str, token: str) -> None:
    if select_hf_repo(repo) == DEFAULT_HF_REPO and not token:
        raise ValueError(
            f"{DEFAULT_HF_REPO} es privado. Introduce un token HF con "
            "permisos de lectura (Resume) y escritura (respaldo), o configura "
            "HF_TOKEN en Kaggle Secrets antes de iniciar Gradio."
        )
