#!/usr/bin/env python3
"""Download a verified Piper Neo Studio build without pip/install steps.

Usable from Kaggle, Colab and other compatible Linux x86_64 notebooks.
Data, checkpoints and HF token are separate and remain under project directory.
"""
import argparse
import hashlib
import os
import pathlib
import platform
import subprocess
import sys
import tarfile
import urllib.request


def download(url: str, output: pathlib.Path):
    with urllib.request.urlopen(url, timeout=90) as inp, output.open("wb") as dst:
        while True:
            chunk = inp.read(8 * 1024 * 1024)
            if not chunk:
                break
            dst.write(chunk)


def sha256(path: pathlib.Path) -> str:
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(8 * 1024 * 1024), b""):
            value.update(chunk)
    return value.hexdigest()


def safe_unpack(archive: pathlib.Path, destination: pathlib.Path) -> pathlib.Path:
    base = destination.resolve()
    with tarfile.open(archive, "r:gz") as obj:
        for member in obj:
            target = (destination / member.name).resolve()
            if target != base and base not in target.parents:
                raise RuntimeError("Unsafe path inside archive: " + member.name)
            if member.issym() or member.islnk() or member.isdev() or member.isfifo():
                raise RuntimeError("Unsupported link/special file in portable archive")
        obj.extractall(destination)
    return destination / "piper-neo-studio" / "run.sh"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--url", required=True, help="Trusted release tar.gz URL")
    parser.add_argument("--sha256", required=True, help="Expected archive digest")
    parser.add_argument("--directory", default="/kaggle/working")
    parser.add_argument("--share", action="store_true")
    parser.add_argument("--port", type=int, default=7860)
    args = parser.parse_args()

    if sys.platform != "linux" or platform.machine() != "x86_64":
        raise SystemExit("Only Linux x86_64 is supported by this build")
    if len(args.sha256) != 64:
        raise SystemExit("A full SHA-256 digest is required for immutable installation")

    destination = pathlib.Path(args.directory)
    destination.mkdir(parents=True, exist_ok=True)
    archive = destination / "piper-neo-studio-runtime.tar.gz"
    print("Downloading runtime from trusted URL", flush=True)
    download(args.url, archive)
    got = sha256(archive)
    if got.lower() != args.sha256.lower():
        archive.unlink(missing_ok=True)
        raise SystemExit("SHA-256 mismatch: download discarded")

    run_script = safe_unpack(archive, destination)
    if not run_script.is_file():
        raise SystemExit("Missing run.sh in archive")
    print("Verified runtime:", run_script, flush=True)
    cmd = ["bash", str(run_script), "--port", str(args.port)]
    if args.share:
        cmd.append("--share")
    os.execv("/bin/bash", cmd)


if __name__ == "__main__":
    main()
