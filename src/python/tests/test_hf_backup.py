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
        self.contents = {}
        self.checkpoint_sha = {}
        self.creates = []
        self.uploads = []
        self.repo_info_calls = []

    def repo_info(self, repo_id, *, repo_type="model", token=None):
        self.repo_info_calls.append((repo_id, repo_type, token))
        return SimpleNamespace(id=repo_id, private=True)

    def create_repo(self, **kw):
        self.creates.append(kw)

    def upload_file(self, *, path_or_fileobj, path_in_repo, repo_id, **kw):
        self.files[path_in_repo] = Path(path_or_fileobj).stat().st_size
        if path_in_repo == "config.json":
            self.contents[path_in_repo] = Path(path_or_fileobj).read_bytes()
        else:
            # The mock validator returns the known fixture SHA "abc".
            self.checkpoint_sha[path_in_repo] = "abc"
        self.uploads.append(path_in_repo)

    def download_file(self, repo_id, filename):
        import tempfile
        path = Path(tempfile.gettempdir()) / "piper_test_hf_remote_config.json"
        path.write_bytes(self.contents[filename])
        return str(path)

    def get_paths_info(self, repo_id, paths, **kw):
        return [
            SimpleNamespace(
                path=path,
                size=self.files[path],
                lfs=(
                    SimpleNamespace(sha256=self.checkpoint_sha[path])
                    if path in self.checkpoint_sha else None
                ),
            )
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

    def test_different_remote_config_size_does_not_block_new_checkpoint(self):
        with tempfile.TemporaryDirectory() as directory:
            project = Path(directory)
            ckpt = write_project(project)
            hub = FakeHf()
            # The previous fine-tune saved a different config.json.
            hub.files["config.json"] = 1234
            hub.contents["config.json"] = b"older settings"
            old_contents = hub.contents["config.json"]
            result = sync_verified_checkpoints(
                project, "test/my-model", "FAKE", api=hub,
                validator=lambda p, c: {
                    "size": p.stat().st_size, "name": p.name,
                    "sha256": "abc", "epoch": 0, "global_step": 84,
                },
            )
            self.assertEqual(result["uploaded"], [ckpt.name])
            self.assertEqual(result["failed"], [])
            self.assertEqual(result["config_status"], "already_remote")
            self.assertNotIn("config.json", hub.uploads)
            self.assertEqual(hub.contents["config.json"], old_contents)

    def test_existing_remote_config_same_size_is_left_untouched(self):
        with tempfile.TemporaryDirectory() as directory:
            project = Path(directory)
            ckpt = write_project(project)
            hub = FakeHf()
            local = (project / "training" / "config.json").read_bytes()
            hub.files["config.json"] = len(local)
            hub.contents["config.json"] = b"x" * len(local)
            result = sync_verified_checkpoints(
                project, "test/my-model", "FAKE", api=hub,
                validator=lambda p, c: {
                    "size": p.stat().st_size, "name": p.name,
                    "sha256": "abc", "epoch": 0, "global_step": 84,
                },
            )
            self.assertEqual(result["uploaded"], [ckpt.name])
            self.assertEqual(result["config_status"], "already_remote")
            self.assertEqual(hub.contents["config.json"], b"x" * len(local))
            self.assertNotIn("config.json", hub.uploads)

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

    def test_old_local_manifest_cannot_hide_empty_remote_repo(self):
        with tempfile.TemporaryDirectory() as directory:
            project = Path(directory)
            ckpt = write_project(project)
            hub = FakeHf()
            def valid(snapshot, config):
                return {
                    "size": snapshot.stat().st_size,
                    "name": snapshot.name, "sha256": "abc",
                    "epoch": 0, "global_step": 84,
                }
            first = sync_verified_checkpoints(
                project, "test/my-model", "FAKE", api=hub, validator=valid
            )
            self.assertEqual(first["uploaded"], [ckpt.name])
            self.assertTrue((project / ".hf_synced_verified.json").exists())
            # User deletes and recreates repo: old local manifest stays.
            hub.files.clear()
            hub.contents.clear()
            hub.checkpoint_sha.clear()
            second = sync_verified_checkpoints(
                project, "test/my-model", "FAKE", api=hub, validator=valid
            )
            self.assertEqual(second["uploaded"], [ckpt.name])
            self.assertEqual(second["skipped"], [])
            self.assertEqual(hub.uploads.count(ckpt.name), 2)
            self.assertEqual(hub.uploads.count("config.json"), 2)

    def test_remote_checkpoint_existing_is_kept_without_overwriting(self):
        with tempfile.TemporaryDirectory() as directory:
            project = Path(directory)
            ckpt = write_project(project)
            hub = FakeHf()
            def valid(snapshot, config):
                return {
                    "size": snapshot.stat().st_size,
                    "name": snapshot.name, "sha256": "abc",
                    "epoch": 0, "global_step": 84,
                }
            first = sync_verified_checkpoints(
                project, "test/my-model", "FAKE", api=hub, validator=valid
            )
            self.assertEqual(first["uploaded"], [ckpt.name])
            second = sync_verified_checkpoints(
                project, "test/my-model", "FAKE", api=hub, validator=valid
            )
            self.assertEqual(second["uploaded"], [])
            self.assertEqual(len(second["skipped"]), 1)
            self.assertEqual(hub.uploads, ["config.json", ckpt.name])

    def test_conflicting_remote_sha_not_overwritten_or_deleted(self):
        with tempfile.TemporaryDirectory() as directory:
            project = Path(directory)
            ckpt = write_project(project)
            hub = FakeHf()
            config = project / "training" / "config.json"
            hub.files["config.json"] = config.stat().st_size
            hub.contents["config.json"] = config.read_bytes()
            hub.files[ckpt.name] = ckpt.stat().st_size
            hub.checkpoint_sha[ckpt.name] = "different-remote-hash"
            result = sync_verified_checkpoints(
                project, "test/my-model", "FAKE", api=hub,
                validator=lambda s,c: {
                    "size": s.stat().st_size, "name": s.name,
                    "sha256": "abc", "epoch": 0, "global_step": 84
                }
            )
            self.assertEqual(result["uploaded"], [])
            self.assertEqual(len(result["failed"]), 1)
            self.assertEqual(hub.uploads, [])
            self.assertIn(ckpt.name, hub.files)

    def test_new_checkpoint_does_not_remove_older_remote_versions(self):
        with tempfile.TemporaryDirectory() as directory:
            project = Path(directory)
            old = write_project(project)
            hub = FakeHf()
            def valid(snapshot, config):
                return {
                    "size": snapshot.stat().st_size,
                    "name": snapshot.name,
                    "sha256": "abc", "epoch": int(snapshot.name.split("-")[0].split("=")[1]),
                    "global_step": int(snapshot.stem.split("step=")[1]),
                }
            sync_verified_checkpoints(project, "test/my-model", "FAKE", api=hub, validator=valid)
            newer = old.with_name("epoch=1-step=170.ckpt")
            with newer.open("wb") as stream:
                stream.truncate(36 * 1024 * 1024)
            prev = time.time() - 60
            os.utime(newer, (prev, prev))
            second = sync_verified_checkpoints(project, "test/my-model", "FAKE", api=hub, validator=valid)
            self.assertEqual(second["uploaded"], [newer.name])
            self.assertIn(old.name, hub.files)
            self.assertIn(newer.name, hub.files)
            self.assertEqual(hub.uploads.count(old.name), 1)

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
