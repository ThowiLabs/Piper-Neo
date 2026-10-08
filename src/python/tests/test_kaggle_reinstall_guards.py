"""Static regressions for safe Kaggle reinstalls; NEVER execute notebook cells."""
import json
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
NOTEBOOK = ROOT / "notebooks" / "KAGGLE_IMPORT_PIPER_NEO_STUDIO.ipynb"


class KaggleReinstallGuards(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        notebook = json.loads(NOTEBOOK.read_text(encoding="utf-8"))
        cls.cells = [
            "".join(cell.get("source", []))
            for cell in notebook["cells"] if cell["cell_type"] == "code"
        ]

    def test_data_root_and_gradio_log_are_external(self):
        source = "\n".join(self.cells)
        self.assertIn('DATA_ROOT = "/kaggle/working/piper_finetune"', source)
        self.assertIn('"PIPER_FINETUNE_ROOT": DATA_ROOT', source)
        self.assertIn('"GRADIO_TEMP_DIR": DATA_ROOT + "/gradio_tmp"', source)
        self.assertIn('web_log = Path(DATA_ROOT) / "studio_kaggle.log"', source)

    def test_clone_is_staged_and_safe_guards_precede_deletion(self):
        cell = next(src for src in self.cells if "# 2. Reinstalación fresca" in src)
        checks = (
            "configured_origin.removesuffix",
            "if found:",
            'ROOT.encode() in command_line',
            "TemporaryDirectory(",
            '["git", "clone"',
        )
        deletion = cell.index("shutil.rmtree(project_dir)")
        for check in checks:
            self.assertIn(check, cell)
            self.assertLess(cell.index(check), deletion)
        self.assertIn("fresh.rename(project_dir)", cell)
        self.assertIn("shutil.copy2(legacy_log, old_log_backup)", cell)


if __name__ == "__main__":
    unittest.main()
