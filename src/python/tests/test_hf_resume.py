"""HF remote resume behavior, without network, tokens, or real 800MB files."""
import json
import shutil
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace

from piper_train.hf_resume import (
    ROOT_SELECTION, _rank_checkpoint, list_remote_versions,
    prepare_resume_from_hf,
)


CONFIG = {
    "num_speakers": 1,
    "num_symbols": 256,
    "phoneme_type": "espeak",
    "phoneme_id_map": {"a": [1], "b": [2]},
    "espeak": {"voice": "es-419"},
    "audio": {"sample_rate": 22050, "quality": "training"},
}


class FakeHub:
    def __init__(self, files):
        self.files = files
        self.downloaded = []

    def list_repo_files(self, repo_id, repo_type="model"):
        return list(self.files)

    def get_paths_info(self, repo_id, paths, repo_type="model", expand=True):
        return [
            SimpleNamespace(path=name, size=len(self.files[name]), lfs=None)
            for name in paths if name in self.files
        ]

    def whoami(self, token=None):
        return {"name": "example-user"}


def data_project(path: Path):
    folder = path / "training"
    folder.mkdir()
    (folder / "dataset.jsonl").write_text('{"x": 1}\n')
    (folder / "config.json").write_text(json.dumps(CONFIG))
    return folder


def fake_download_factory(hub):
    def download(repo_id, name, target, token):
        target.mkdir(parents=True, exist_ok=True)
        p = target / name
        p.write_bytes(hub.files[name])
        hub.downloaded.append(name)
        return p
    return download


def valid_ckpt(path, config):
    if path.read_bytes() == b"broken":
        raise ValueError("corrupt")
    name = path.name
    import re
    match = re.match(r"epoch=(\d+)-step=(\d+)\.ckpt", name)
    return {
        "name": name,
        "size": path.stat().st_size,
        "epoch": int(match[1]),
        "global_step": int(match[2]),
        "sha256": "abcd",
        "status": "structurally_valid",
    }


class HfResumeTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.project = Path(self.tmp.name)
        data_project(self.project)
        self.files = {
            "config.json": json.dumps(CONFIG).encode(),
            "epoch=10-step=400.ckpt": b"old but good",
            "epoch=3-step=700.ckpt": b"broken",
            "epoch=4-step=550.ckpt": b"good",
            "notes/foo.ckpt": b"ignored",
            "last.ckpt": b"ignored",
        }
        self.hub = FakeHub(self.files)
        self.downloader = fake_download_factory(self.hub)

    def tearDown(self):
        self.tmp.cleanup()

    def test_latest_by_step_not_epoch_or_filename(self):
        self.assertGreater(
            _rank_checkpoint("epoch=3-step=700.ckpt"),
            _rank_checkpoint("epoch=10-step=400.ckpt"),
        )
        repo, names = list_remote_versions("model-name", "FAKE", api=self.hub)
        self.assertEqual(repo, "example-user/model-name")
        self.assertEqual(names[0], "epoch=3-step=700.ckpt")
        self.assertNotIn("last.ckpt", names)

    def test_latest_invalid_falls_back_to_previous_valid(self):
        report = prepare_resume_from_hf(
            self.project, "example-user/model-name", "FAKE",
            api=self.hub, download=self.downloader,
            validator=valid_ckpt, max_epochs=100,
        )
        self.assertEqual(report["filename"], "epoch=4-step=550.ckpt")
        self.assertEqual(report["global_step"], 550)
        self.assertEqual(report["epoch"], 4)
        self.assertEqual(len(report["fallback_errors"]), 1)
        self.assertEqual(
            json.loads((self.project / "training" / "config.json").read_text()),
            CONFIG,
        )
        self.assertEqual(
            json.loads((Path(report["download_path"]).parent / ".last_resume.json").read_text())["filename"],
            report["filename"],
        )

    def test_explicit_corrupt_does_not_silently_use_previous(self):
        with self.assertRaisesRegex(ValueError, "corrupt"):
            prepare_resume_from_hf(
                self.project, "example-user/model-name", "FAKE",
                selected_checkpoint="epoch=3-step=700.ckpt",
                api=self.hub, download=self.downloader, validator=valid_ckpt,
            )
        self.assertNotIn("epoch=4-step=550.ckpt", self.hub.downloaded)

    def test_mismatched_config_does_not_touch_local_dataset(self):
        config = dict(CONFIG)
        config["espeak"] = {"voice": "es_ES"}
        self.hub.files["config.json"] = json.dumps(config).encode()
        with self.assertRaisesRegex(ValueError, "eSpeak"):
            prepare_resume_from_hf(
                self.project, "example-user/model-name", "FAKE",
                api=self.hub, download=self.downloader, validator=valid_ckpt
            )
        self.assertEqual(
            json.loads((self.project / "training" / "config.json").read_text()),
            CONFIG,
        )

    def test_missing_dataset_blocks_resume(self):
        (self.project / "training" / "dataset.jsonl").unlink()
        with self.assertRaisesRegex(ValueError, "dataset.jsonl"):
            prepare_resume_from_hf(
                self.project, "example-user/model-name", "FAKE",
                api=self.hub, download=self.downloader, validator=valid_ckpt
            )

    def test_explicit_checkpoint_selection(self):
        report = prepare_resume_from_hf(
            self.project, "example-user/model-name", "FAKE",
            selected_checkpoint="epoch=10-step=400.ckpt",
            api=self.hub, download=self.downloader, validator=valid_ckpt,
            max_epochs=100
        )
        self.assertEqual(report["filename"], "epoch=10-step=400.ckpt")
        self.assertNotIn("epoch=3-step=700.ckpt", self.hub.downloaded)


if __name__ == "__main__":
    unittest.main()
