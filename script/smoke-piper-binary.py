#!/usr/bin/env python3
"""Smoke test for the final Piper Neo binary.

The script intentionally uses only the Python standard library so it can run on
fresh Windows/Linux runners after the project build.

Examples:
  python script/smoke-piper-binary.py --models models
  python script/smoke-piper-binary.py --binary dist-winlibs/piper-neo-windows/piper.exe --models models
  python script/smoke-piper-binary.py --models C:\\voices --text "Hola desde Piper Neo"
"""

from __future__ import annotations

import argparse
import json
import os
import platform
import shutil
import socket
import subprocess
import sys
import tempfile
import threading
import time
import urllib.error
import urllib.request
from pathlib import Path
from typing import Any, Iterable


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_TEXT = "Hola, esto es una prueba automatica de Piper Neo."


class SmokeFailure(RuntimeError):
    pass


def log(message: str) -> None:
    print(f"[smoke] {message}", flush=True)


def fail(message: str) -> None:
    raise SmokeFailure(message)


def is_windows() -> bool:
    return platform.system().lower().startswith("win")


def executable_name() -> str:
    return "piper.exe" if is_windows() else "piper"


def candidate_binaries() -> list[Path]:
    exe = executable_name()
    candidates = [
        ROOT / "dist-winlibs" / "piper-neo-windows" / exe,
        ROOT / "dist" / exe,
        ROOT / "build" / exe,
        ROOT / "build" / "piper" / exe,
        ROOT / "build-windows" / exe,
        ROOT / "piper" / exe,
        ROOT / exe,
    ]
    if is_windows():
        candidates.extend(ROOT.glob(f"dist*/**/{exe}"))
    else:
        candidates.extend(ROOT.glob(f"build*/**/{exe}"))
    return list(dict.fromkeys(candidates))


def resolve_binary(value: str | None) -> Path:
    if value:
        binary = Path(value).expanduser().resolve()
        if not binary.exists():
            fail(f"No existe el binario indicado: {binary}")
        return binary

    for candidate in candidate_binaries():
        if candidate.exists() and candidate.is_file():
            return candidate.resolve()

    path_hit = shutil.which(executable_name()) or shutil.which("piper")
    if path_hit:
        return Path(path_hit).resolve()

    fail("No encontre piper.exe/piper. Usa --binary para indicar la ruta.")


def run_command(cmd: list[str], timeout: int = 60, input_text: str | None = None) -> subprocess.CompletedProcess[str]:
    log("$ " + " ".join(str(part) for part in cmd))
    completed = subprocess.run(
        cmd,
        input=input_text,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=timeout,
        cwd=str(ROOT),
    )
    if completed.returncode != 0:
        print(completed.stdout)
        print(completed.stderr, file=sys.stderr)
        fail(f"Comando fallo con codigo {completed.returncode}: {' '.join(cmd)}")
    return completed



def run_binary_command(cmd: list[str], timeout: int = 60,
                       input_bytes: bytes | None = None) -> subprocess.CompletedProcess[bytes]:
    log("$ " + " ".join(str(part) for part in cmd))
    completed = subprocess.run(
        cmd,
        input=input_bytes,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=timeout,
        cwd=str(ROOT),
    )
    if completed.returncode != 0:
        sys.stdout.buffer.write(completed.stdout)
        sys.stderr.buffer.write(completed.stderr)
        fail(f"Comando fallo con codigo {completed.returncode}: {' '.join(cmd)}")
    return completed


def find_voice_model(models_dir: Path) -> tuple[Path, Path | None]:
    if not models_dir.exists():
        fail(f"No existe --models: {models_dir}")

    neo_models = sorted(models_dir.rglob("*.neo"))
    if neo_models:
        return neo_models[0], None

    for model in sorted(models_dir.rglob("*.onnx")):
        candidates = [Path(str(model) + ".json"), model.with_suffix(".onnx.json"), model.with_suffix(".json")]
        for config in candidates:
            if config.exists():
                return model, config

    fail(f"No encontre modelos .neo ni pares .onnx + .json dentro de {models_dir}")


def assert_wav(path: Path) -> None:
    if not path.exists():
        fail(f"No se genero WAV: {path}")
    data = path.read_bytes()
    if len(data) < 44:
        fail(f"WAV demasiado pequeno: {path} ({len(data)} bytes)")
    if data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        fail(f"El archivo no parece WAV RIFF valido: {path}")
    log(f"WAV OK: {path.name} ({len(data)} bytes)")


def free_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.bind(("127.0.0.1", 0))
        return int(sock.getsockname()[1])


def http_json(method: str, url: str, payload: dict[str, Any] | None = None,
              token: str | None = None, timeout: int = 20) -> tuple[int, Any]:
    body = None
    headers = {"Accept": "application/json"}
    if payload is not None:
        body = json.dumps(payload).encode("utf-8")
        headers["Content-Type"] = "application/json"
    if token:
        headers["Authorization"] = f"Bearer {token}"

    request = urllib.request.Request(url, data=body, method=method, headers=headers)
    try:
        with urllib.request.urlopen(request, timeout=timeout) as response:
            raw = response.read()
            if not raw:
                return response.status, None
            return response.status, json.loads(raw.decode("utf-8"))
    except urllib.error.HTTPError as exc:
        raw = exc.read()
        try:
            parsed = json.loads(raw.decode("utf-8")) if raw else None
        except Exception:
            parsed = raw.decode("utf-8", errors="replace")
        return exc.code, parsed


def http_bytes(url: str, token: str | None = None, timeout: int = 20) -> tuple[int, bytes]:
    headers = {}
    if token:
        headers["Authorization"] = f"Bearer {token}"
    request = urllib.request.Request(url, method="GET", headers=headers)
    try:
        with urllib.request.urlopen(request, timeout=timeout) as response:
            return response.status, response.read()
    except urllib.error.HTTPError as exc:
        return exc.code, exc.read()


def wait_for_health(base_url: str, token: str | None, timeout_seconds: int) -> None:
    deadline = time.time() + timeout_seconds
    last_error: Exception | None = None
    while time.time() < deadline:
        try:
            status, payload = http_json("GET", f"{base_url}/api/health", token=token, timeout=3)
            if status == 200:
                log(f"API health OK: {payload}")
                return
        except Exception as exc:  # pragma: no cover - diagnostic path
            last_error = exc
        time.sleep(0.3)
    if last_error:
        fail(f"La API no respondio /api/health: {last_error}")
    fail("La API no respondio /api/health dentro del timeout")


def start_server(binary: Path, models_dir: Path, output_dir: Path, token: str | None,
                 timeout: int) -> tuple[subprocess.Popen[str], str]:
    port = free_port()
    cmd = [
        str(binary),
        "--server",
        "--host", "127.0.0.1",
        "--port", str(port),
        "--models", str(models_dir),
        "--output_dir", str(output_dir),
        "--models-refresh-seconds", "1",
        "--output-retention-seconds", "60",
        "--queue-timeout-seconds", "30",
        "--max-input-bytes", "1048576",
        "--quiet",
    ]
    if token:
        cmd.extend(["--api-token", token])

    log("Iniciando API: " + " ".join(cmd))
    process = subprocess.Popen(
        cmd,
        cwd=str(ROOT),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    base_url = f"http://127.0.0.1:{port}"
    try:
        wait_for_health(base_url, token, timeout)
    except Exception:
        stop_server(process)
        stdout, stderr = process.communicate(timeout=3)
        print(stdout)
        print(stderr, file=sys.stderr)
        raise
    return process, base_url


def stop_server(process: subprocess.Popen[str]) -> None:
    if process.poll() is not None:
        return
    process.terminate()
    try:
        process.wait(timeout=8)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=5)


def append_config(cmd: list[str], config: Path | None) -> list[str]:
    if config:
        cmd.extend(["--config", str(config)])
    return cmd


def assert_raw_audio(data: bytes, label: str) -> None:
    if len(data) < 256:
        fail(f"Salida RAW demasiado pequena en {label}: {len(data)} bytes")
    if data[:4] == b"RIFF" or data[8:12] == b"WAVE":
        fail(f"{label} esperaba audio RAW, pero parece WAV")
    if all(byte == 0 for byte in data[: min(len(data), 4096)]):
        fail(f"{label} contiene solo ceros al inicio")
    log(f"RAW OK: {label} ({len(data)} bytes)")


def newest_wav(directory: Path) -> Path:
    wavs = sorted(directory.glob("*.wav"), key=lambda path: path.stat().st_mtime, reverse=True)
    if not wavs:
        fail(f"No se genero ningun WAV en {directory}")
    return wavs[0]


def test_cli(binary: Path, model: Path, config: Path | None, text: str, work_dir: Path,
             timeout: int, skip_raw: bool) -> None:
    run_command([str(binary), "--help"], timeout=timeout)
    run_command([str(binary), "--version"], timeout=timeout)

    output = work_dir / "cli-text.wav"
    cmd = append_config([str(binary), "--model", str(model), "--text", text,
                         "--output_file", str(output), "--quiet"], config)
    run_command(cmd, timeout=timeout)
    assert_wav(output)

    input_file = work_dir / "cli-input.txt"
    input_file.write_text(text + "\nSegunda linea desde archivo.", encoding="utf-8")
    output = work_dir / "cli-input-file.wav"
    cmd = append_config([str(binary), "--model", str(model), "--input_file", str(input_file),
                         "--output_file", str(output), "--quiet"], config)
    run_command(cmd, timeout=timeout)
    assert_wav(output)

    output = work_dir / "cli-stdin.wav"
    cmd = append_config([str(binary), "--model", str(model), "--output_file", str(output),
                         "--quiet"], config)
    run_command(cmd, timeout=timeout, input_text=text + "\nTexto desde stdin.\n")
    assert_wav(output)

    output = work_dir / "cli-json.wav"
    json_line = json.dumps({"text": text, "output_file": str(output)}, ensure_ascii=False) + "\n"
    cmd = append_config([str(binary), "--model", str(model), "--json-input", "--quiet"], config)
    run_command(cmd, timeout=timeout, input_text=json_line)
    assert_wav(output)

    output_dir = work_dir / "cli-output-dir"
    output_dir.mkdir(parents=True, exist_ok=True)
    cmd = append_config([str(binary), "--model", str(model), "--text", text,
                         "--output_dir", str(output_dir), "--quiet"], config)
    run_command(cmd, timeout=timeout)
    assert_wav(newest_wav(output_dir))

    if not skip_raw:
        cmd = append_config([str(binary), "--model", str(model), "--output_raw", "--quiet"], config)
        completed = run_binary_command(cmd, timeout=timeout,
                                       input_bytes=(text + "\n").encode("utf-8"))
        assert_raw_audio(completed.stdout, "cli-output-raw")


def run_tts_request(base_url: str, text: str, token: str | None, timeout: int,
                    output_path: Path) -> None:
    status, tts_payload = http_json("POST", f"{base_url}/api/v1/tts", {"text": text}, token=token, timeout=timeout)
    if status != 201:
        fail(f"/api/v1/tts devolvio {status}: {tts_payload}")
    data = tts_payload.get("data") if isinstance(tts_payload, dict) else None
    file_url = data.get("url") if isinstance(data, dict) else None
    if not file_url:
        fail(f"Respuesta TTS sin data.url: {tts_payload}")
    status, wav_data = http_bytes(f"{base_url}{file_url}", token=token, timeout=timeout)
    if status != 200:
        fail(f"Descarga de WAV devolvio {status}")
    output_path.write_bytes(wav_data)
    assert_wav(output_path)


def run_api_stress(base_url: str, text: str, token: str | None, timeout: int,
                   work_dir: Path, requests: int) -> None:
    if requests <= 0:
        return

    errors: list[str] = []
    lock = threading.Lock()

    def worker(index: int) -> None:
        try:
            long_text = (text.strip() + " ") * 120
            run_tts_request(base_url, long_text.strip(), token, timeout,
                            work_dir / f"api-stress-{index}.wav")
        except Exception as exc:  # pragma: no cover - diagnostic path
            with lock:
                errors.append(f"stress #{index}: {exc}")

    threads = [threading.Thread(target=worker, args=(index,), daemon=True)
               for index in range(requests)]
    for thread in threads:
        thread.start()
    for thread in threads:
        thread.join(timeout + 10)

    if errors:
        fail("; ".join(errors))
    log(f"API stress OK: {requests} request(s) concurrentes")


def test_api(binary: Path, models_dir: Path, text: str, work_dir: Path, token: str | None,
             timeout: int, stress_requests: int) -> None:
    process, base_url = start_server(binary, models_dir, work_dir / "api-output", token, timeout)
    try:
        if token:
            status, _ = http_json("GET", f"{base_url}/api/v1/models", token=None, timeout=timeout)
            if status != 401:
                fail(f"Se esperaba 401 sin token; se obtuvo {status}")
            log("Auth negativa OK")

        for path in ["/api/health", "/api/v1/status", "/api/v1/metrics"]:
            status, payload = http_json("GET", f"{base_url}{path}", token=token, timeout=timeout)
            if status != 200:
                fail(f"{path} devolvio {status}: {payload}")
            log(f"{path} OK")

        status, models_payload = http_json("GET", f"{base_url}/api/v1/models", token=token, timeout=timeout)
        if status != 200:
            fail(f"/api/v1/models devolvio {status}: {models_payload}")
        log("/api/v1/models OK")

        status, invalid_payload = http_json("POST", f"{base_url}/api/v1/tts", {"text": ""}, token=token, timeout=timeout)
        if status != 400:
            fail(f"JSON invalido/missing fields esperaba 400; obtuvo {status}: {invalid_payload}")
        log("/api/v1/tts validacion negativa OK")

        run_tts_request(base_url, text, token, timeout, work_dir / "api-test.wav")
        log("/api/v1/tts OK")

        run_api_stress(base_url, text, token, timeout, work_dir, stress_requests)
    finally:
        stop_server(process)


def parse_args(argv: Iterable[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Prueba smoke del binario final de Piper Neo")
    parser.add_argument("--binary", help="Ruta a piper.exe/piper. Si se omite, se busca automaticamente.")
    parser.add_argument("--models", required=True, help="Directorio con modelos .neo o .onnx + .json")
    parser.add_argument("--text", default=DEFAULT_TEXT, help="Texto de prueba para CLI y API")
    parser.add_argument("--api-token", default="", help="Token opcional para probar API protegida")
    parser.add_argument("--timeout", type=int, default=180, help="Timeout por operacion en segundos")
    parser.add_argument("--skip-cli", action="store_true", help="No probar sintesis CLI")
    parser.add_argument("--skip-api", action="store_true", help="No levantar/probar API HTTP")
    parser.add_argument("--skip-cli-raw", action="store_true", help="No probar --output_raw en CLI")
    parser.add_argument("--keep-temp", action="store_true", help="Conservar carpeta temporal de resultados")
    parser.add_argument("--stress-api-requests", type=int, default=2,
                        help="Peticiones TTS concurrentes de texto largo para detectar problemas de eSpeak/model cache")
    return parser.parse_args(list(argv))


def main(argv: Iterable[str]) -> int:
    args = parse_args(argv)
    binary = resolve_binary(args.binary)
    models_dir = Path(args.models).expanduser().resolve()
    model, config = find_voice_model(models_dir)

    log(f"Binario: {binary}")
    log(f"Modelo CLI: {model}")
    if config:
        log(f"Config CLI: {config}")

    temp_root = Path(tempfile.mkdtemp(prefix="piper-neo-smoke-"))
    log(f"Temp: {temp_root}")
    try:
        if not args.skip_cli:
            test_cli(binary, model, config, args.text, temp_root, args.timeout, args.skip_cli_raw)
        if not args.skip_api:
            token = args.api_token.strip() or None
            test_api(binary, models_dir, args.text, temp_root, token, args.timeout, args.stress_api_requests)
        log("SMOKE_OK")
        return 0
    finally:
        if args.keep_temp:
            log(f"Temp conservado: {temp_root}")
        else:
            shutil.rmtree(temp_root, ignore_errors=True)


if __name__ == "__main__":
    try:
        raise SystemExit(main(sys.argv[1:]))
    except SmokeFailure as exc:
        print(f"SMOKE_FAIL: {exc}", file=sys.stderr)
        raise SystemExit(1)
