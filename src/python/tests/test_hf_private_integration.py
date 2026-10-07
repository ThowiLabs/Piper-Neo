"""HF auth, private resume, watcher credential renewal and fallback transport."""
import os
import tempfile
import time
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

from piper_train import hf_backup, hf_resume, hf_watcher
from piper_train.hf_settings import (
    DEFAULT_HF_REPO, hf_auth_token, select_hf_repo,
    require_hf_auth_for_private_repo,
)
from test_hf_backup import FakeHf, write_project


class AuthAndTransportTests(unittest.TestCase):
    def test_repo_default_and_read_credentials(self):
        self.assertEqual(select_hf_repo(""), DEFAULT_HF_REPO)
        self.assertEqual(select_hf_repo(None), DEFAULT_HF_REPO)
        self.assertEqual(select_hf_repo("other/private"), "other/private")
        with patch.dict(os.environ, {"HF_TOKEN": "local-auth"}, clear=False):
            self.assertEqual(hf_auth_token(""), "local-auth")
            self.assertEqual(hf_auth_token("ui-auth"), "ui-auth")
        with self.assertRaisesRegex(ValueError, "privado"):
            require_hf_auth_for_private_repo(DEFAULT_HF_REPO, "")

    def test_remote_model_repo_not_created_if_existing_private(self):
        fake = FakeHf()
        result = hf_backup._ensure_remote_model_repo(
            fake, DEFAULT_HF_REPO, "write-auth", True
        )
        self.assertEqual(result, "already_exists")
        self.assertEqual(fake.creates, [])
        self.assertEqual(fake.repo_info_calls[0][2], "write-auth")

    def test_large_api_upload_error_uses_cli_fallback(self):
        with tempfile.TemporaryDirectory() as directory:
            project = Path(directory)
            ckpt = write_project(project)
            fake = FakeHf()
            original_upload = fake.upload_file
            def flaky_upload(*, path_or_fileobj, path_in_repo, repo_id, **kwargs):
                if path_in_repo == ckpt.name:
                    raise RuntimeError("Error while uploading to the Hub")
                return original_upload(
                    path_or_fileobj=path_or_fileobj, path_in_repo=path_in_repo,
                    repo_id=repo_id, **kwargs,
                )
            def fake_cli(repo_id, filepath, auth):
                self.assertEqual(auth, "test-auth")
                original_upload(
                    path_or_fileobj=str(filepath),
                    path_in_repo=filepath.name, repo_id=repo_id
                )
            fake.upload_file = flaky_upload
            with patch.object(
                hf_backup, "_cli_upload_checkpoint", side_effect=fake_cli
            ) as cli:
                report = hf_backup.sync_verified_checkpoints(
                    project, "user/model", "test-auth", api=fake,
                    validator=lambda p,c: {
                        "size": p.stat().st_size, "name": p.name,
                        "sha256": "abc", "epoch": 0, "global_step": 84,
                    },
                )
            self.assertEqual(report["uploaded"], [ckpt.name])
            self.assertEqual(report["failed"], [])
            cli.assert_called_once()
            self.assertEqual(fake.uploads, ["config.json", ckpt.name])

    def test_cli_auth_only_in_environment_not_arguments(self):
        with tempfile.TemporaryDirectory() as folder:
            file = Path(folder) / "epoch=1-step=20.ckpt"
            file.write_bytes(b"x")
            response = SimpleNamespace(returncode=0, stdout="ok", stderr="")
            with patch.object(hf_backup.subprocess, "run", return_value=response) as run:
                hf_backup._cli_upload_checkpoint("user/model", file, "test-auth")
            args, kwargs = run.call_args
            command = args[0]
            self.assertNotIn("test-auth", " ".join(map(str, command)))
            self.assertEqual(kwargs["env"]["HF_TOKEN"], "test-auth")
            self.assertNotIn("--delete", command)
            self.assertNotIn("--force", command)

    def test_watcher_hot_swaps_token_without_restarting_training(self):
        class FakeThread:
            def is_alive(self):
                return True
        class Hub:
            def __init__(self, token):
                self.token = token
            def whoami(self, token):
                assert token == self.token
                return {"name": "HirCoir"}
        watcher = hf_watcher.BackupWatcher()
        project = Path("/tmp/piper-test-project")
        watcher.details = {
            "project": project, "repo_id": DEFAULT_HF_REPO,
            "token": "old-auth", "private": True, "interval": 60
        }
        watcher.thread = FakeThread()
        with patch.object(hf_watcher, "HfApi", Hub):
            status = watcher.start(
                project, DEFAULT_HF_REPO, "fresh-auth", True, 25
            )
        self.assertIn("Credenciales HF renovadas", status)
        self.assertEqual(watcher.details["token"], "fresh-auth")
        self.assertEqual(watcher.details["interval"], 25)
        self.assertTrue(watcher.running())

    def test_known_private_repo_404_is_never_recreated(self):
        from huggingface_hub.utils import RepositoryNotFoundError
        from requests import Response
        hub = FakeHf()
        response = Response()
        response.status_code = 404
        response.url = "https://huggingface.co/api/models/example"
        def missing(**kwargs):
            raise RepositoryNotFoundError("Not Found", response=response)
        hub.repo_info = missing
        with self.assertRaisesRegex(PermissionError, "no se intentará recrearlo"):
            hf_backup._ensure_remote_model_repo(
                hub, DEFAULT_HF_REPO, "read-only-demo", True
            )
        self.assertEqual(hub.creates, [])

    def test_check_hf_access_reports_read_but_not_claim_write(self):
        from piper_train import gradio_finetune as ui
        class FakeApi:
            def __init__(self, token):
                self.token = token
            def whoami(self, token):
                return {"name": "HirCoir"}
            def repo_info(self, **kwargs):
                return SimpleNamespace(private=True)
        with patch.object(ui, "HfApi", FakeApi):
            result = ui.verify_hf_private_access(DEFAULT_HF_REPO, "demo-auth")
        self.assertIn("Permiso Read: verificado", result)
        self.assertIn("solo se comprueba", result)
        self.assertNotIn("demo-auth", result)

    def test_private_repo_without_token_reports_read_requirement(self):
        with patch.dict(os.environ, {"HF_TOKEN": ""}):
            with self.assertRaisesRegex(ValueError, "privado"):
                hf_resume.list_remote_versions(DEFAULT_HF_REPO, "")


if __name__ == "__main__":
    unittest.main()
