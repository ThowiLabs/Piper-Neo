"""Show dataset failures inside Gradio, including where and why they failed."""
import os
import subprocess
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest.mock import patch

from piper_train import gradio_finetune as ui


class DatasetDiagnosticTests(unittest.TestCase):
    def test_matplotlib_inline_environment_is_corrected_before_gradio_import(self):
        env = {
            **os.environ,
            "MPLBACKEND": "module://matplotlib_inline.backend_inline",
        }
        code = (
            "import os; "
            "from piper_train.gradio_finetune import build_ui; "
            "import matplotlib; "
            "assert os.environ['MPLBACKEND']=='Agg'; "
            "assert matplotlib.get_backend().lower()=='agg'; "
            "assert build_ui().blocks; "
            "print('INLINE_BACKEND_FALLBACK_OK')"
        )
        result = subprocess.run(
            [sys.executable, "-c", code],
            env=env, capture_output=True, text=True, timeout=40,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("INLINE_BACKEND_FALLBACK_OK", result.stdout)

    def test_bad_zip_is_reported_inline_and_saved(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            corrupted = root / "broken.zip"
            corrupted.write_bytes(b"not a real zip file")
            with patch.object(ui, "KAGGLE_ROOT", root):
                report, detail, download = ui.prepare_dataset_with_diagnostics(
                    "fresh-voice", "", str(corrupted), None, False
                )
            self.assertIn("ERROR en: Abrir y extraer el ZIP", report)
            self.assertIn("dataset debe ser un ZIP", report)
            self.assertIn("Traceback", detail)
            self.assertIsNotNone(download)
            log = Path(download)
            self.assertTrue(log.exists())
            self.assertIn("Abrir y extraer", log.read_text())

    def test_missing_csv_is_reported_in_validation_panel(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            archive = root / "dataset.zip"
            with zipfile.ZipFile(archive, "w") as out:
                out.writestr("wav/001.wav", b"sound")
            with patch.object(ui, "KAGGLE_ROOT", root):
                report, detail, download = ui.prepare_dataset_with_diagnostics(
                    "voice2", "", str(archive), None, False
                )
            self.assertIn("ERROR en: Localizar CSV", report)
            self.assertIn("CSV/metadata", report)
            self.assertIsNotNone(download)
            self.assertIn("Traceback", detail)

    def test_valid_dataset_reports_success_and_log(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            archive = root / "dataset.zip"
            with zipfile.ZipFile(archive, "w") as out:
                out.writestr("wav/001.wav", b"sound")
            metadata = root / "metadata.csv"
            metadata.write_text("001|Hola mundo\n", encoding="utf-8")
            with patch.object(ui, "KAGGLE_ROOT", root):
                report, detail, download = ui.prepare_dataset_with_diagnostics(
                    "voice3", "", str(archive), str(metadata), False
                )
            self.assertIn("VALIDACIÓN COMPLETADA", report)
            self.assertIn("Filas utilizables: 1", report)
            self.assertIn("Correcto", detail)
            self.assertEqual(
                (root / "voice3" / "input" / "metadata.csv").read_text().strip(),
                "001.wav|Hola mundo",
            )
            self.assertIn("RESULTADO: CORRECTO", Path(download).read_text())

    def test_failed_download_redacts_url_query_and_tokens(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            url = "https://example.com/private.zip?token=private-SECRET"
            def broken(*args, **kw):
                raise RuntimeError(
                    f"401 fetching {url} auth hf_SUPERSECRET1234567890"
                )
            with patch.object(ui, "KAGGLE_ROOT", root), patch.object(
                ui, "_download_source", side_effect=broken
            ):
                report, detail, download = ui.prepare_dataset_with_diagnostics(
                    "voice4", url, None, None, False
                )
            self.assertIn("ERROR en: Localizar ZIP", report)
            for visible in (report, detail, Path(download).read_text()):
                self.assertNotIn("private-SECRET", visible)
                self.assertNotIn("hf_SUPERSECRET1234567890", visible)
            self.assertIn("RuntimeError", report)

    def test_can_read_logs_from_gradio_without_a_new_upload(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with patch.object(ui, "KAGGLE_ROOT", root):
                ui.prepare_dataset_with_diagnostics(
                    "voice5", "", None, None, False
                )
                text, path = ui.latest_dataset_diagnostics("voice5")
            self.assertIn("ETAPA", text)
            self.assertTrue(Path(path).exists())

    def test_live_training_guard_is_rendered_as_error(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with patch.object(ui, "KAGGLE_ROOT", root), patch.object(
                ui, "_other_training_pids", return_value=[1234]
            ):
                report, detail, download = ui.prepare_dataset_with_diagnostics(
                    "voice6", "", None, None, True
                )
            self.assertIn("ERROR en: Comprobar entrenamiento", report)
            self.assertIn("1234", report)
            self.assertTrue(Path(download).exists())


if __name__ == "__main__":
    unittest.main()
