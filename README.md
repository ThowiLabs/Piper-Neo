<p align="center">
  <img src="assets/branding/piper-neo-banner.png" alt="Piper Neo banner" width="100%">
</p>

# Piper Neo

Piper Neo is a steroid-enhanced version of [rhasspy/piper](https://github.com/rhasspy/piper): same local-first TTS spirit, but with stronger CLI/server ergonomics, safer long-text handling, `.neo` voice packages, resource controls and a cleaner C++ core.

This repository is focused on the Piper Neo engine itself: C++ CLI, local HTTP API, model packaging, text normalization, Python training/runtime utilities and documentation. Desktop applications and external UI clients are intentionally not included.

## Highlights

- Local neural text-to-speech with classic Piper `.onnx` voices.
- Single-file Piper Neo voice packages: `.neo`.
- Direct text input with `--text`.
- Text file input with `--input_file` / `--input-file`.
- Smart UTF-8 chunking for long texts.
- Stable Latin American Spanish input with accents, `ñ`, `¿?`, `¡!` and punctuation-aware splitting.
- Progressive WAV writing for long inputs.
- Local HTTP API server with `--server`.
- Model directory support with `--models`.
- Per-request model selection in the JSON API.
- Optional API token from `--api-token`, environment variables or `.env`.
- Auto resource configuration for CPU, RAM, queue size, workers and replicas.
- Fair chunk scheduler so long requests do not monopolize the engine.
- Temporary output limits and automatic cleanup.
- Model metadata endpoints without exposing absolute internal paths.
- Configurable text normalization per model for URLs, emails, currency, percentages, versions and replacements.

## Repository scope

Included:

```text
src/cpp/          Piper Neo C++ engine, CLI, API server and .neo packages
src/python/       Training utilities inherited from Piper
src/python_run/   Python runtime utilities inherited from Piper
script/           Build/smoke helper scripts
TTS/bin/          Resampling helper for dataset preparation
docs/             Technical documentation
contexto/         Current project context for maintainers
neo-docs/         Piper Neo package format notes
notebooks/        Training/inference notebooks
models/           Placeholder for local voices; actual models are ignored
```

Not included:

```text
apps/             Desktop/UI applications were removed from this public engine repo
build*/dist*/     Local build outputs are ignored
models/*.onnx     Downloaded voices are local artifacts and are ignored
outputs/          Generated API audio is runtime data and is ignored
```

## Quick CLI usage

Generate from direct text:

```bash
./piper --model models/es_MX-voice.onnx   --text "Hola México, ¿cómo estás?"   --output_file saludo.wav
```

Generate from a UTF-8 text file:

```bash
./piper --model models/es_MX-voice.onnx   --input_file texto.txt   --output_file salida.wav   --cpu-threads 2
```

Use a `.neo` package directly:

```bash
./piper --model models/es_MX-Veritasium.neo   --text "Hello from Piper Neo"   --output_file hello.wav
```

## Quick API usage

Start the local server with automatic resource tuning:

```bash
./piper --server --models models
```

Recommended local server example:

```bash
./piper --server   --models models   --host 127.0.0.1   --port 8080   --cpu-profile auto   --output-retention-seconds 3600
```

Generate TTS:

```bash
curl -X POST http://127.0.0.1:8080/api/v1/tts   -H "Content-Type: application/json"   -d '{"model":"es_MX-voice.onnx","text":"Hola México, ¿cómo estás? ñ á é í ó ú"}'
```

Download the generated audio from the `url` returned by the API:

```bash
curl http://127.0.0.1:8080/api/v1/files/tts_xxx.wav --output audio.wav
```

## API endpoints

- `GET /api/health`
- `GET /api/v1/status`
- `GET /api/v1/metrics`
- `GET /api/v1/models`
- `GET /api/v1/models?include=metadata`
- `GET /api/v1/models?include=technical`
- `GET /api/v1/models/{model}/image`
- `POST /api/v1/tts`
- `GET /api/v1/files/{file}`

Success responses use this shape:

```json
{
  "success": true,
  "message": "Audio generado exitosamente.",
  "data": {}
}
```

Error responses use:

```json
{
  "success": false,
  "error": "invalid_request",
  "message": "Error description."
}
```

## Optional API token

If no token is configured, the server does not require authentication.

Configure a token with one of these options:

```bash
./piper --server --models models --api-token "secret"
```

```env
PIPER_API_TOKEN=secret
```

Request with:

```bash
curl http://127.0.0.1:8080/api/v1/status   -H "Authorization: Bearer secret"
```

`X-API-Token: secret` is also accepted.

## Resource controls

Auto mode is enabled by default in server mode. It detects CPU affinity, Docker/cgroup CPU quota and memory limit before choosing workers, jobs and model replicas. These flags can override it:

```bash
--cpu-profile auto|eco|balanced|fast|max
--cpu-threads NUM|auto
--max-concurrent-jobs NUM
--chunk-workers NUM
--max-model-replicas NUM
--queue-size NUM
--queue-timeout-seconds NUM
--max-input-bytes NUM
--max-text-chunk-bytes NUM
--max-temp-bytes NUM
--output-retention-seconds NUM
--models-refresh-seconds NUM
```

## Piper Neo model packages (`.neo`)

A `.neo` file contains:

- the ONNX model payload;
- the Piper JSON voice config metadata;
- model card information;
- optional image/cover art;
- speaker and inference metadata.

Classic Piper voices remain supported:

```text
voice.onnx
voice.onnx.json
```

Export a classic ONNX voice to `.neo`:

```bash
./piper   --model models/es_MX-Veritasium.onnx   --config models/es_MX-Veritasium.onnx.json   --export-neo models/es_MX-Veritasium.neo
```

Optional cover image:

```bash
./piper   --model models/es_MX-Veritasium.onnx   --config models/es_MX-Veritasium.onnx.json   --neo-image cover.jpg   --export-neo models/es_MX-Veritasium.neo
```

When running the API server, `models/` may contain both `.onnx` voices and `.neo` packages:

```bash
./piper --server --models models
```

## Build

### Windows local build

Use the existing Windows build script:

```bat
py script\build-windows.py clean
py script\build-windows.py
```

### GitHub Actions

The public workflows are located at:

```text
.github/workflows/build.yml
.github/workflows/build-release.yml
```

**Build Piper Neo** runs automatically on every `push`/pull request targeting `main` and only publishes a workflow artifact.

**Build Release Piper Neo** is manual-only. Run it from the Actions tab when you want to compile Windows amd64, create or update a GitHub Release and upload `piper_windows_amd64.zip`.

### Generic CMake build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DPIPER_BUILD_TESTS=OFF
cmake --build build --config Release
cmake --install build
```

### Docker CLI/server image

```bash
docker compose -f docker-compose-cli.yml build
```

## Smoke checks

```bash
python3 script/smoke-text-normalizer.py
python3 script/smoke-project-structure.py
cmake -S . -B /tmp/piper-neo-cmake-check -DPIPER_BUILD_TESTS=OFF
cmake -S . -B /tmp/piper-neo-cmake-check-tests -DPIPER_BUILD_TESTS=ON
cmake --build /tmp/piper-neo-cmake-check-tests --target test_neo_package
cmake --build /tmp/piper-neo-cmake-check-tests --target test_text_sanitizer
cmake --build /tmp/piper-neo-cmake-check-tests --target test_markup_parser
ctest --test-dir /tmp/piper-neo-cmake-check-tests -R "test_neo_package|test_text_sanitizer|test_markup_parser|test_http_parser" --output-on-failure
# After building the final binary, test CLI + API with real models:
python script/smoke-piper-binary.py --models models
# You can also pass the binary explicitly:
python script/smoke-piper-binary.py --binary dist-winlibs/piper-neo-windows/piper.exe --models models
python script/smoke-piper-binary.py --binary dist-winlibs/piper-neo-windows/piper.exe --models models --stress-api-requests 4
```

## Internal C++ architecture

The C++ core is split by responsibility:

- `src/cpp/main.cpp`: minimal binary entry point.
- `src/cpp/app/`: CLI parsing, help text, validation, run configuration, hardware probing, resource policy, environment resolution and execution modes for CLI/server/export/synthesis.
- `src/cpp/piper.hpp` and `src/cpp/piper/`: compatible public facade, public types and engine API.
- `src/cpp/core/`: Piper runtime, voice loading, ONNX inference, UTF-8 safe chunking, synthesis pipeline and WAV streaming.
- `src/cpp/server.cpp`: local HTTP server runtime, sockets and accept loop.
- `src/cpp/server/`: HTTP, auth, route dispatch, text sanitization, models, cache/runtime/loader, TTS scheduler, metrics, markup TTS, WAV, jobs and cleanup modules.
- `src/cpp/server/routes/`: HTTP API routes split by responsibility: health/status/metrics, models/images, generated files and TTS synthesis.
- `src/cpp/server/sanitize/`: API text sanitizer internals for UTF-8/Unicode, content filters and risk scoring.
- `src/cpp/text_normalizer.cpp` and `src/cpp/text/`: configurable model text normalization.
- `src/cpp/neo_model.cpp` and `src/cpp/neo/`: public facade and internal modules for reading, inspection, extraction and writing of `.neo` packages.

More details are available in `docs/core-architecture.md` and `contexto/`.

## Documentation

- `README.es.md`: Spanish documentation.
- `docs/api-server.md`: HTTP API documentation.
- `docs/new-piper-usage.md`: CLI usage, text files and smart chunking.
- `docs/resource-management-plan.md`: resource management notes.
- `docs/text-normalization.md`: model text normalization.
- `docs/text-preprocessing.md`: server-side TTS sanitizer.
- `src/cpp/tests/test_text_sanitizer.cpp`: functional sanitizer coverage for URLs, emails, markup, code, emojis and invalid UTF-8.
- `src/cpp/tests/test_markup_parser.cpp`: functional markup parser coverage for `<model>`, `<silence>`, speaker ids and request options.
- `docs/markup-tts.md`: local multi-voice markup.
- `neo-docs/neo-format.md`: `.neo` package format.

## Upstream

Piper Neo is derived from [rhasspy/piper](https://github.com/rhasspy/piper). Respect the upstream license and attribution when redistributing binaries or source code.


### HTTP modular y catálogo de modelos

El servidor de Piper Neo separa rutas, parser HTTP, socket I/O, respuestas, sanitizer, markup TTS y catálogo/cache de modelos en módulos internos pequeños. Esto permite mantener la API TTS sin convertir `server.cpp` o `request_handler.cpp` en archivos monolíticos.
