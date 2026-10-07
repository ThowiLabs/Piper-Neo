"""Background Hugging Face backup watcher independent from the training owner.

Keeps the token only in server memory: a browser reload doesn't stop uploads.
The watcher can follow a training process started by an older Gradio server.
"""
from __future__ import annotations

import os
import threading
import time
from pathlib import Path

from .hf_backup import sync_verified_checkpoints

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
                f"Última sincronización: {self.last_sync or 'pendiente'}. "
                f"{self.last_report}"
            )

    def start(self, project: Path, repo_id: str, token: str,
              private: bool, interval: int):
        token = (token or os.environ.get("HF_TOKEN", "")).strip()
        if not repo_id or not repo_id.strip():
            raise ValueError("Falta el repositorio HF (usuario/modelo).")
        if not token:
            raise ValueError("Falta token HF o variable HF_TOKEN.")
        with self.lock:
            if self.running():
                if (self.details["project"] == project and
                        self.details["repo_id"] == repo_id.strip()):
                    return self.status()
                raise ValueError(
                    "Ya hay un respaldo activo para otro proyecto o repo. "
                    "Detén el respaldo anterior para reconfigurarlo."
                )
            self.stop_event = threading.Event()
            self.details = {
                "project": Path(project), "repo_id": repo_id.strip(),
                "token": token, "private": bool(private),
                "interval": max(30, int(interval)),
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
                    f"Subidos: {len(report['uploaded'])}; "
                    f"sin cambios: {len(report['skipped'])}; "
                    f"fallos: {len(report['failed'])}."
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
                with (details["project"] / "hf_sync.log").open(
                    "a", encoding="utf-8"
                ) as log:
                    log.write(f"[{self.last_sync}] {message}\n")
            except Exception as exc:
                with self.lock:
                    self.last_report = f"ERROR: {exc}"
                    self.last_sync = time.strftime("%Y-%m-%d %H:%M:%S")
            if self.stop_event.wait(details["interval"]):
                break


BACKUP_WATCHER = BackupWatcher()
