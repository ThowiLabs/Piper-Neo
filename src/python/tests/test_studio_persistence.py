"""Regression tests for persistent web controls and active project protections."""
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from piper_train import studio_state
from piper_train import gradio_finetune as web


class PersistenceTests(unittest.TestCase):
    def test_hf_resume_builds_full_resume_command_without_starting_trainer(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            training = root / "capibara" / "training"
            training.mkdir(parents=True)
            (training / "dataset.jsonl").write_text("{}\n")
            (training / "config.json").write_text("{}")
            source = root / "valid.ckpt"
            source.write_bytes(b"mocked")
            hf_report = {
                "repository": "user/voice",
                "filename": "epoch=5-step=2860.ckpt",
                "download_path": str(source),
                "epoch": 5,
                "global_step": 2860,
                "sha256": "abcdef012345",
            }
            with patch.object(web, "KAGGLE_ROOT", root), \
                 patch.object(web, "_other_training_pids", return_value=[]), \
                 patch("piper_train.hf_resume.prepare_resume_from_hf", return_value=hf_report) as remote, \
                 patch.object(web.JOB, "start") as spawn:
                message = web.start_training(
                    "capibara", "Resume de corrida", "", None, None, "",
                    8, 1000, 5, 15, 400, "cpu",
                    "", "", True, "", 20, False,
                    "Hugging Face: último válido", "user/voice",
                    "Automático: último checkpoint válido",
                )
            command = spawn.call_args.args[0]
            self.assertIn("--resume_from_checkpoint", command)
            self.assertEqual(command[command.index("--resume_from_checkpoint")+1], str(source))
            self.assertNotIn("--init-from-checkpoint", command)
            self.assertIn("epoch=5-step=2860.ckpt", message)
            self.assertEqual(remote.call_args.kwargs["max_epochs"], 1000)

    def test_start_stop_visibility_reflects_real_process_state(self):
        with patch.object(web, "_other_training_pids", return_value=[4321]):
            start, stop, message = web.training_buttons("capibara")
            self.assertFalse(start["visible"])
            self.assertTrue(stop["visible"])
            self.assertIn("4321", message)
        with patch.object(web, "_other_training_pids", return_value=[]):
            start, stop, message = web.training_buttons("capibara")
            self.assertTrue(start["visible"])
            self.assertFalse(stop["visible"])

    def test_preferences_survive_page_reload_and_hide_secrets(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            values = {
                "hf_repo_id": "example/voice",
                "batch_size": 12,
                "inference_text": "¿Qué tal, México?",
                "checkpoint_minutes": 15,
                "language": "es-419",
                "hf_token": "NEVER_SAVE_ME",
            }
            studio_state.save_preferences(root, "capibara", values)
            name, restored = studio_state.load_preferences(root)
            self.assertEqual(name, "capibara")
            self.assertEqual(restored["batch_size"], 12)
            self.assertEqual(restored["inference_text"], "¿Qué tal, México?")
            self.assertEqual(restored["hf_repo_id"], "example/voice")
            self.assertNotIn("hf_token", restored)
            self.assertNotIn("NEVER_SAVE_ME", (root / "capibara" / ".studio_preferences.json").read_text())

    def test_two_projects_are_isolated_and_last_project_recovers(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            studio_state.save_preferences(root, "voice1", {"batch_size": 8})
            studio_state.save_preferences(root, "voice2", {"batch_size": 16})
            self.assertEqual(studio_state.load_preferences(root)[0], "voice2")
            self.assertEqual(studio_state.load_preferences(root, "voice1")[1]["batch_size"], 8)

    def test_gradio_restore_returns_server_artefacts(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with patch.object(web, "KAGGLE_ROOT", root):
                studio_state.save_preferences(root, "capibara", {"batch_size": 12})
                training = root / "capibara" / "training"
                training.mkdir()
                (training / "config.json").write_text('{"hello":"world"}')
                restored = web.load_ui_preferences(include_name=True)
                self.assertEqual(restored[0], "capibara")
                self.assertEqual(len(restored), len(studio_state.FIELDS) + 9)
                self.assertIn("config.json", restored[-7][0])
                self.assertEqual(restored[studio_state.FIELDS.index("batch_size")+1], 12)

    def test_guards_external_process_before_deleting_data(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with patch.object(web, "KAGGLE_ROOT", root):
                project = root / "capibara"
                project.mkdir()
                (project / "marker.txt").write_text("must remain")
                with patch.object(web, "_other_training_pids", return_value=[12345]):
                    with self.assertRaises(Exception):
                        web.prepare_dataset("capibara", "", None, None, True)
                self.assertEqual((project / "marker.txt").read_text(), "must remain")


if __name__ == "__main__":
    unittest.main()
