"""Background Hugging Face backup watcher independent from the training owner.

Keeps the token only in server memory: a browser reload doesn't stop uploads.
The watcher can follow a training process started by an older Gradio server.
"""
from __future__ import annotations

import os
import threading
import time
from pathlib import Path

from .hf_backup import resolve_repo_id, sync_verified_checkpoints
from .hf_settings import hf_auth_token, select_hf_repo, require_hf_auth_for_private_repo
from huggingface_hub import HfApi

_SYNC_LOCK = threading.Lock()


class BackupWatcher:
    def __init__(self):
        self.lock = threading.RLock()
        self.stop_event = threading.Event()
        self.thread = None
        self.details = None
        self.last_report = ""
        self.last_sync = None

    def running(self):
        with self.lock:
            return self.thread is not None and self.thread.is_alive()

    def status(self):
        with self.lock:
            if not self.running():
                return "Respaldo automático: desactivado."
            return (
                "Respaldo automático: ACTIVO (sin tocar el entrenamiento). "
                f"Proyecto: {self.details['project'].name}. "
                f"HF: {self.details['repo_id']}. "
                f"Intervalo: {self.details['interval']} s. "
                f"Última revisión del repo HF: {self.last_sync or 'pendiente'}. "
                f"{self.last_report}"
            )

    def start(self, project: Path, repo_id: str, token: str,
              private: bool, interval: int):
        token = hf_auth_token(token)
        repo_id = select_hf_repo(repo_id)
        require_hf_auth_for_private_repo(repo_id, token)
        if not token:
            raise ValueError("Para respaldo HF automático hace falta token Write o HF_TOKEN.")
        # An explicit namespace is not evidence that a token is still valid.
        # Verify the real token before storing it for any automatic transfers.
        api = HfApi(token=token)
        api.whoami(token=token)
        resolved_repo = resolve_repo_id(repo_id, token, api=api)
        with self.lock:
            if self.running():
                if (self.details["project"] == Path(project) and
                        self.details["repo_id"] == resolved_repo):
                    if self.stop_event.is_set():
                        raise ValueError(
                            "El respaldo aún se está deteniendo. "
                            "Espera a que cambie a desactivado y actívalo de nuevo."
                        )
                    # KEY FIX: a new token for the SAME repo must replace the
                    # old token held in server RAM without stopping training.
                    refreshed = self.details["token"] != token
                    self.details.update({
                        "token": token, "private": bool(private),
                        "interval": max(15, int(interval)),
                    })
                    if refreshed:
                        self.last_report = "Credenciales HF renovadas; próximo respaldo usa el token nuevo."
                    return self.status()
                raise ValueError(
                    "Ya hay un respaldo activo para otro proyecto o repo. "
                    "Detén el respaldo anterior para reconfigurarlo."
                )
            self.stop_event = threading.Event()
            self.details = {
                "project": Path(project), "repo_id": resolved_repo,
                "token": token, "private": bool(private),
                "interval": max(15, int(interval)),
            }
            self.last_report = ""
            self.last_sync = None
            self.thread = threading.Thread(
                target=self._loop, name="piper-hf-backup-watcher", daemon=True
            )
            self.thread.start()
        return self.status()

    def stop(self):
        with self.lock:
            if not self.running():
                return "No existe respaldo automático activo."
            self.stop_event.set()
            return "Se solicitó detener el respaldo HF. El entrenamiento continúa."

    def _loop(self):
        details = self.details
        while True:
            try:
                with _SYNC_LOCK:
                    report = sync_verified_checkpoints(
                        details["project"], details["repo_id"], details["token"],
                        private=details["private"], remote_prefix="",
                    )
                message = (
                    f"Respaldos NUEVOS en HF: {len(report['uploaded'])}; "
                    f"ya existentes remotamente: {len(report['skipped'])}; "
                    f"rechazados: {len(report['failed'])}; "
                    f"config.json: {report.get('config_status', 'pendiente')}."
                )
                if report["failed"]:
                    message += " " + "; ".join(
                        f"{r['file']}: {r['reason']}" for r in report["failed"][:3]
                    )
                with self.lock:
                    self.last_report = message
                    self.last_sync = time.strftime("%Y-%m-%d %H:%M:%S")
                (details["project"] / "hf_sync.log").parent.mkdir(
                    parents=True, exist_ok=True
                )
                with (details["project"] / "hf_backup.log").open(
                    "a", encoding="utf-8"
                ) as log:
                    log.write(f"[{self.last_sync}] {message}\n")
            except Exception as exc:
                with self.lock:
                    self.last_report = f"ERROR: {type(exc).__name__}: {exc}"
                    self.last_sync = time.strftime("%Y-%m-%d %H:%M:%S")
                try:
                    path = details["project"] / "hf_backup.log"
                    path.parent.mkdir(parents=True, exist_ok=True)
                    with path.open("a", encoding="utf-8") as log:
                        log.write(f"[{self.last_sync}] {self.last_report}\n")
                except OSError:
                    pass
            if self.stop_event.wait(details["interval"]):
                break


BACKUP_WATCHER = BackupWatcher()
