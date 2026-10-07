"""Tests for safe HF upload orchestration (no real network or token)."""
from __future__ import annotations
import json
import os
import tempfile
import time
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

from piper_train.hf_backup import sync_verified_checkpoints, resolve_repo_id


class FakeHf:
    def __init__(self):
        self.files = {}
        self.creates = []
        self.uploads = []

    def create_repo(self, **kw):
        self.creates.append(kw)

    def upload_file(self, *, path_or_fileobj, path_in_repo, repo_id, **kw):
        self.files[path_in_repo] = Path(path_or_fileobj).stat().st_size
        self.uploads.append(path_in_repo)

    def get_paths_info(self, repo_id, paths, **kw):
        return [
            SimpleNamespace(path=path, size=self.files[path])
            for path in paths if path in self.files
        ]


def write_project(root: Path):
    training = root / "training"
    ckpt_dir = training / "lightning_logs" / "version_0" / "checkpoints"
    ckpt_dir.mkdir(parents=True, exist_ok=True)
    (training / "config.json").write_text(json.dumps({
        "num_speakers": 1, "num_symbols": 256,
        "audio": {"sample_rate": 22050}
    }), encoding="utf-8")
    checkpoint = ckpt_dir / "epoch=0-step=84.ckpt"
    with checkpoint.open("wb") as f:
        f.truncate(36 * 1024 * 1024)
    old = time.time() - 35
    os.utime(checkpoint, (old, old))
    return checkpoint


class VerifiedBackupTests(unittest.TestCase):
    def test_short_hf_name_resolves_from_token(self):
        hf = FakeHf()
        hf.whoami = lambda **kw: {"name": "my-hf-user"}
        self.assertEqual(
            resolve_repo_id("capibara-medium", "FAKE", api=hf),
            "my-hf-user/capibara-medium",
        )
        self.assertEqual(
            resolve_repo_id("namespace/capibara-medium", "FAKE", api=hf),
            "namespace/capibara-medium",
        )
        with self.assertRaises(ValueError):
            resolve_repo_id("../capibara-medium", "FAKE", api=hf)

    def test_repo_with_existing_checkpoint_cannot_be_overwritten(self):
        with tempfile.TemporaryDirectory() as directory:
            project = Path(directory)
            checkpoint = write_project(project)
            hf = FakeHf()
            hf.files[checkpoint.name] = 12345
            def valid(snapshot, config):
                return {
                    "size": snapshot.stat().st_size,
                    "name": snapshot.name, "sha256": "abc",
                    "epoch": 0, "global_step": 84
                }
            report = sync_verified_checkpoints(
                project, "test/my-model", "FAKE", api=hf, validator=valid
            )
            self.assertEqual(report["uploaded"], [])
            self.assertEqual(len(report["failed"]), 1)
            self.assertNotIn(checkpoint.name, hf.uploads)

    def test_root_name_and_manifest_prevent_reupload(self):
        with tempfile.TemporaryDirectory() as directory:
            project = Path(directory)
            checkpoint = write_project(project)
            hf = FakeHf()

            def valid(snapshot, config):
                self.assertEqual(snapshot.name, checkpoint.name)
                return {
                    "size": snapshot.stat().st_size,
                    "name": snapshot.name,
                    "sha256": "abc", "epoch": 0, "global_step": 84,
                    "status": "structurally_valid",
                }

            first = sync_verified_checkpoints(
                project, "test/my-model", "FAKE", api=hf, validator=valid
            )
            self.assertEqual(first["uploaded"], [checkpoint.name])
            self.assertEqual(hf.uploads, ["config.json", checkpoint.name])
            again = sync_verified_checkpoints(
                project, "test/my-model", "FAKE", api=hf, validator=valid
            )
            self.assertEqual(again["uploaded"], [])
            self.assertEqual(len(hf.uploads), 2)

    def test_rejected_checkpoint_never_uploaded(self):
        with tempfile.TemporaryDirectory() as directory:
            project = Path(directory)
            write_project(project)
            hf = FakeHf()

            def broken(snapshot, config):
                raise ValueError("Broken pickle contents")

            result = sync_verified_checkpoints(
                project, "test/my-model", "FAKE", api=hf, validator=broken
            )
            self.assertEqual(result["uploaded"], [])
            self.assertEqual(len(result["failed"]), 1)
            self.assertEqual(hf.uploads, [])
            self.assertEqual(hf.creates, [])

    def test_missing_confirmation_not_marked_uploaded(self):
        with tempfile.TemporaryDirectory() as directory:
            project = Path(directory)
            write_project(project)
            hf = FakeHf()
            hf.get_paths_info = lambda *a, **kw: []
            def valid(snapshot, config):
                return {
                    "size": snapshot.stat().st_size,
                    "name": snapshot.name, "sha256": "abc",
                    "epoch": 0, "global_step": 84
                }

            report = sync_verified_checkpoints(
                project, "test/my-model", "FAKE", api=hf, validator=valid
            )
            self.assertFalse(report["uploaded"])
            self.assertEqual(len(report["failed"]), 1)
            self.assertFalse((project / ".hf_synced_verified.json").exists())

if __name__ == "__main__":
    unittest.main()
