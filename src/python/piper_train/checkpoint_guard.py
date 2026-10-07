"""Checkpoint integrity checks used BEFORE a Piper fine-tune backup is published.

Only validate checkpoints produced by a trusted local training job. Loading
arbitrary PyTorch <= 1.13 checkpoints is UNSAFE because torch.load uses pickle.
Run this module as a separate process without Hugging Face credentials.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import re
import sys
import time
from pathlib import Path

import torch

CHECKPOINT_NAME = re.compile(r"^epoch=(\d+)-step=(\d+)(?:-v\d+)?\.ckpt$")
MIN_BYTES = 32 * 1024 * 1024
MIN_TENSORS = 100


def stable_file(path: Path, min_age: float = 12.0) -> tuple[int, int]:
    if not path.is_file() or path.is_symlink():
        raise ValueError("Checkpoint is missing or not a regular file")
    info = path.stat()
    if info.st_size < MIN_BYTES:
        raise ValueError(f"Checkpoint is suspiciously small: {info.st_size} bytes")
    if time.time() - info.st_mtime < min_age:
        raise ValueError("Checkpoint is still being written (not old enough)")
    return info.st_size, info.st_mtime_ns


def validate_checkpoint(path: Path, config_path: Path, min_age: float = 12.0) -> dict:
    """Validate a complete Lightning checkpoint and compatible Piper config.

    This checks structure, architecture, tensors and numeric metadata, plus file
    stability and a full SHA-256. It does NOT guarantee speech quality.
    """
    path = Path(path)
    config_path = Path(config_path)
    name_match = CHECKPOINT_NAME.fullmatch(path.name)
    if not name_match:
        raise ValueError("Expected original epoch=N-step=N.ckpt name")
    before = stable_file(path, min_age=min_age)
    if not config_path.is_file():
        raise ValueError("Missing config.json")
    config = json.loads(config_path.read_text(encoding="utf-8"))
    for field in ("num_symbols", "num_speakers"):
        if not isinstance(config.get(field), int) or config[field] < 1:
            raise ValueError(f"Invalid config.json: {field}")
    sample_rate = config.get("audio", {}).get("sample_rate")
    if not isinstance(sample_rate, int) or sample_rate < 8000:
        raise ValueError("Invalid config.json: audio.sample_rate")

    # WARNING: pickle. This must only inspect trusted local checkpoints.
    checkpoint = torch.load(str(path), map_location="cpu")
    if not isinstance(checkpoint, dict):
        raise ValueError("Checkpoint is not a Lightning state dictionary")
    epoch, step = checkpoint.get("epoch"), checkpoint.get("global_step")
    if not isinstance(epoch, int) or epoch < 0 or not isinstance(step, int) or step < 0:
        raise ValueError("Missing/invalid epoch or global_step")
    if epoch != int(name_match[1]) or step != int(name_match[2]):
        raise ValueError(f"Filename epoch/step mismatch: {epoch}/{step}")
    state_dict = checkpoint.get("state_dict")
    if not isinstance(state_dict, dict) or len(state_dict) < MIN_TENSORS:
        raise ValueError("Checkpoint state_dict is missing or incomplete")
    generator_keys = [key for key in state_dict if key.startswith("model_g.")]
    discriminator_keys = [key for key in state_dict if key.startswith("model_d.")]
    if len(generator_keys) < 50 or len(discriminator_keys) < 20:
        raise ValueError("Missing generator/discriminator tensors")

    hparams = checkpoint.get("hyper_parameters")
    if not isinstance(hparams, dict):
        raise ValueError("Missing Lightning hyper_parameters")
    for field in ("num_symbols", "num_speakers"):
        if int(hparams.get(field, -1)) != config[field]:
            raise ValueError(f"Checkpoint/config mismatch: {field}")
    if int(hparams.get("sample_rate", -1)) != sample_rate:
        raise ValueError("Checkpoint/config mismatch: sample_rate")

    opt_states = checkpoint.get("optimizer_states")
    if not isinstance(opt_states, list) or len(opt_states) != 2:
        raise ValueError("Incomplete optimizer states for resumable training")

    for name, tensor in state_dict.items():
        if not isinstance(tensor, torch.Tensor):
            raise ValueError(f"Not a tensor: {name}")
        if tensor.numel() == 0:
            raise ValueError(f"Empty tensor: {name}")
        if tensor.is_floating_point() and not bool(torch.isfinite(tensor).all()):
            raise ValueError(f"Nonfinite tensor: {name}")

    del checkpoint
    digest = hashlib.sha256()
    with path.open("rb") as inp:
        while True:
            data = inp.read(8 * 1024 * 1024)
            if not data:
                break
            digest.update(data)
    after = stable_file(path, min_age=min_age)
    if before != after:
        raise ValueError("Checkpoint changed while being validated")

    return {
        "name": path.name,
        "epoch": epoch,
        "global_step": step,
        "size": before[0],
        "sha256": digest.hexdigest(),
        "tensors": len(state_dict),
        "sample_rate": sample_rate,
        "num_speakers": config["num_speakers"],
        "status": "structurally_valid",
    }


def main() -> None:
    parser = argparse.ArgumentParser(description="Validate trusted Piper checkpoint")
    parser.add_argument("checkpoint")
    parser.add_argument("config")
    parser.add_argument("--min-age", type=float, default=12.0)
    args = parser.parse_args()
    try:
        print(json.dumps(validate_checkpoint(
            Path(args.checkpoint), Path(args.config), args.min_age
        ), ensure_ascii=False), flush=True)
    except Exception as error:
        print(json.dumps({"error": str(error)}), flush=True)
        sys.exit(1)


if __name__ == "__main__":
    main()
