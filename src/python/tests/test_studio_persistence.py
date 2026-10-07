"""Regression tests for persistent web controls and active project protections."""
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from piper_train import studio_state
from piper_train import gradio_finetune as web


class PersistenceTests(unittest.TestCase):
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
                self.assertEqual(len(restored), 27)
                self.assertIn("config.json", restored[-4][0])
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
