#!/usr/bin/env python3
"""Smoke test estructural del core Piper Neo.

No compila dependencias nativas. Verifica que el repo público siga sin apps,
que CMake referencie los módulos refactorizados y que contexto/ conserve la
bitácora técnica limpia.
"""
from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

REQUIRED_FILES = [
    "src/cpp/piper.cpp",
    "src/cpp/piper.hpp",
    "src/cpp/piper/types.hpp",
    "src/cpp/piper/api.hpp",
    "src/cpp/core/model_runtime.cpp",
    "src/cpp/core/piper_runtime.cpp",
    "src/cpp/core/sentence_splitter.hpp",
    "src/cpp/core/sentence_splitter.cpp",
    "src/cpp/core/sentence/boundary_detector.hpp",
    "src/cpp/core/sentence/boundary_detector.cpp",
    "src/cpp/core/synthesis_pipeline.cpp",
    "src/cpp/core/pipeline/phonemizer.hpp",
    "src/cpp/core/pipeline/phonemizer.cpp",
    "src/cpp/core/pipeline/phrase_synthesizer.hpp",
    "src/cpp/core/pipeline/phrase_synthesizer.cpp",
    "src/cpp/core/pipeline/text_processing.hpp",
    "src/cpp/core/pipeline/text_processing.cpp",
    "src/cpp/core/synthesis_utils.hpp",
    "src/cpp/core/synthesis_utils.cpp",
    "src/cpp/core/text_chunker.hpp",
    "src/cpp/core/text_chunker.cpp",
    "src/cpp/core/text/chunk_rules.hpp",
    "src/cpp/core/text/chunk_rules.cpp",
    "src/cpp/core/text/utf8_utils.hpp",
    "src/cpp/core/text/utf8_utils.cpp",
    "src/cpp/core/voice_loader.cpp",
    "src/cpp/core/wav_stream_writer.cpp",
    "src/cpp/core/wav/wav_header_writer.cpp",
    "src/cpp/core/wav/wav_header_writer.hpp",
    "src/cpp/core/wav/stream_chunks.cpp",
    "src/cpp/core/wav/stream_chunks.hpp",
    "src/cpp/neo/binary_io.cpp",
    "src/cpp/neo/compression.cpp",
    "src/cpp/neo/file_utils.cpp",
    "src/cpp/neo/image_payload.cpp",
    "src/cpp/neo/package_reader.cpp",
    "src/cpp/neo/package_writer.cpp",
    "src/cpp/neo_model.cpp",
    "src/cpp/app/cli_validation.cpp",
    "src/cpp/app/cli_validation.hpp",
    "src/cpp/app/help_text.cpp",
    "src/cpp/app/help_text.hpp",
    "src/cpp/app/export_neo_mode.cpp",
    "src/cpp/app/export_neo_mode.hpp",
    "src/cpp/app/hardware.cpp",
    "src/cpp/app/hardware_probe.hpp",
    "src/cpp/app/hardware_probe.cpp",
    "src/cpp/app/resource_limits.hpp",
    "src/cpp/app/resource_limits.cpp",
    "src/cpp/app/resource_policy.hpp",
    "src/cpp/app/resource_policy.cpp",
    "src/cpp/app/server_mode.cpp",
    "src/cpp/app/platform_console.cpp",
    "src/cpp/app/platform_paths.cpp",
    "src/cpp/app/server_mode.hpp",
    "src/cpp/app/platform_console.cpp",
    "src/cpp/app/platform_paths.cpp",
    "src/cpp/app/synthesis_input.cpp",
    "src/cpp/app/synthesis_input.hpp",
    "src/cpp/app/synthesis_json.cpp",
    "src/cpp/app/synthesis_json.hpp",
    "src/cpp/app/synthesis_mode.cpp",
    "src/cpp/app/synthesis_mode.hpp",
    "src/cpp/app/synthesis_output.cpp",
    "src/cpp/app/synthesis_output.hpp",
    "src/cpp/app/synthesis_paths.cpp",
    "src/cpp/app/synthesis_paths.hpp",
    "src/cpp/app/voice_runtime.cpp",
    "src/cpp/app/voice_runtime.hpp",
    "src/cpp/server/metrics_report.hpp",
    "src/cpp/server/metrics_report.cpp",
    "src/cpp/server/jobs/chunk_worker.hpp",
    "src/cpp/server/jobs/chunk_worker.cpp",
    "src/cpp/server/jobs/chunked_wav.hpp",
    "src/cpp/server/jobs/chunked_wav.cpp",
    "src/cpp/server/jobs/job_lifecycle.hpp",
    "src/cpp/server/jobs/job_lifecycle.cpp",
    "src/cpp/server/jobs/job_state.hpp",
    "src/cpp/server/sanitize_result.hpp",
    "src/cpp/server/sanitize_result.cpp",
    "src/cpp/server/sanitize/content_filters.hpp",
    "src/cpp/server/sanitize/content_filters.cpp",
    "src/cpp/server/sanitize/risk_score.hpp",
    "src/cpp/server/sanitize/risk_score.cpp",
    "src/cpp/server/sanitize/utf8_text.hpp",
    "src/cpp/server/sanitize/utf8_text.cpp",
    "src/cpp/server/markup/markup_parser.hpp",
    "src/cpp/server/markup/markup_parser.cpp",
    "src/cpp/server/markup/request_options.hpp",
    "src/cpp/server/markup/request_options.cpp",
    "src/cpp/server/markup/audio_parts.hpp",
    "src/cpp/server/markup/audio_parts.cpp",
    "src/cpp/server/routes/route_context.hpp",
    "src/cpp/server/routes/health_routes.hpp",
    "src/cpp/server/routes/health_routes.cpp",
    "src/cpp/server/routes/model_routes.hpp",
    "src/cpp/server/routes/model_routes.cpp",
    "src/cpp/server/routes/file_routes.hpp",
    "src/cpp/server/routes/file_routes.cpp",
    "src/cpp/server/routes/tts_routes.hpp",
    "src/cpp/server/routes/tts_request.hpp",
    "src/cpp/server/routes/tts_payload.hpp",
    "src/cpp/server/routes/tts_payload.cpp",
    "src/cpp/server/routes/tts_request.cpp",
    "src/cpp/server/routes/tts_routes.cpp",
    "src/cpp/server/http_types.hpp",
    "src/cpp/server/http/response_writer.cpp",
    "src/cpp/server/http/socket_io.cpp",
    "src/cpp/server/http/url.cpp",
    "src/cpp/server/media/data_image.cpp",
    "src/cpp/server/media/data_image.hpp",
    "src/cpp/server/media/base64.cpp",
    "src/cpp/server/media/base64.hpp",
    "src/cpp/server/model_cache.cpp",
    "src/cpp/server/model_loader.cpp",
    "src/cpp/server/model_runtime.cpp",
    "src/cpp/server/model_metadata.cpp",
    "src/cpp/server/model_paths.cpp",
    "src/cpp/server/model_scanner.cpp",
    "src/cpp/tests/test_neo_package.cpp",
    "src/cpp/tests/test_text_sanitizer.cpp",
    "src/cpp/tests/test_markup_parser.cpp",
    "src/cpp/tests/test_http_parser.cpp",
    "src/cpp/tests/test_text_chunker.cpp",
    "src/cpp/tests/test_resource_policy.cpp",
    "src/cpp/tests/test_tts_request.cpp",
    "src/cpp/tests/test_sentence_splitter.cpp",
    "src/cpp/tests/test_model_registry.cpp",
    "src/cpp/tests/test_text_builtins.cpp",
    "src/cpp/text/builtin_matchers.cpp",
    "src/cpp/text/builtin_matchers.hpp",
    "src/cpp/text/builtin_renderers.cpp",
    "src/cpp/text/builtin_renderers.hpp",
    "src/cpp/text/protected_segments.cpp",
    "src/cpp/text/protected_segments.hpp",
    "script/smoke-piper-binary.py",
]

REQUIRED_CMAKE_SOURCES = [
    "src/cpp/core/model_runtime.cpp",
    "src/cpp/core/piper_runtime.cpp",
    "src/cpp/core/sentence/boundary_detector.cpp",
    "src/cpp/core/sentence_splitter.cpp",
    "src/cpp/core/synthesis_pipeline.cpp",
    "src/cpp/core/pipeline/phonemizer.cpp",
    "src/cpp/core/pipeline/phrase_synthesizer.cpp",
    "src/cpp/core/pipeline/text_processing.cpp",
    "src/cpp/core/synthesis_utils.cpp",
    "src/cpp/core/text/chunk_rules.cpp",
    "src/cpp/core/text/utf8_utils.cpp",
    "src/cpp/core/text_chunker.cpp",
    "src/cpp/core/voice_loader.cpp",
    "src/cpp/core/wav_stream_writer.cpp",
    "src/cpp/core/wav/wav_header_writer.cpp",
    "src/cpp/core/wav/stream_chunks.cpp",
    "src/cpp/neo/binary_io.cpp",
    "src/cpp/neo/compression.cpp",
    "src/cpp/neo/file_utils.cpp",
    "src/cpp/neo/image_payload.cpp",
    "src/cpp/neo/package_reader.cpp",
    "src/cpp/neo/package_writer.cpp",
    "src/cpp/neo_model.cpp",
    "src/cpp/app/cli_validation.cpp",
    "src/cpp/app/help_text.cpp",
    "src/cpp/app/hardware_probe.cpp",
    "src/cpp/app/resource_limits.cpp",
    "src/cpp/app/resource_policy.cpp",
    "src/cpp/app/export_neo_mode.cpp",
    "src/cpp/app/server_mode.cpp",
    "src/cpp/app/platform_console.cpp",
    "src/cpp/app/platform_paths.cpp",
    "src/cpp/app/synthesis_input.cpp",
    "src/cpp/app/synthesis_json.cpp",
    "src/cpp/app/synthesis_mode.cpp",
    "src/cpp/app/synthesis_output.cpp",
    "src/cpp/app/synthesis_paths.cpp",
    "src/cpp/app/voice_runtime.cpp",
    "src/cpp/server/metrics_report.cpp",
    "src/cpp/server/jobs/chunk_worker.cpp",
    "src/cpp/server/jobs/chunked_wav.cpp",
    "src/cpp/server/jobs/job_lifecycle.cpp",
    "src/cpp/server/sanitize_result.cpp",
    "src/cpp/server/sanitize/content_filters.cpp",
    "src/cpp/server/sanitize/risk_score.cpp",
    "src/cpp/server/sanitize/utf8_text.cpp",
    "src/cpp/server/markup/markup_parser.cpp",
    "src/cpp/server/markup/request_options.cpp",
    "src/cpp/server/markup/audio_parts.cpp",
    "src/cpp/server/routes/health_routes.cpp",
    "src/cpp/server/routes/model_routes.cpp",
    "src/cpp/server/routes/file_routes.cpp",
    "src/cpp/server/routes/tts_payload.cpp",
    "src/cpp/server/routes/tts_request.cpp",
    "src/cpp/server/routes/tts_routes.cpp",
    "src/cpp/server/http_types.hpp",
    "src/cpp/server/http/response_writer.cpp",
    "src/cpp/server/http/socket_io.cpp",
    "src/cpp/server/http/url.cpp",
    "src/cpp/server/media/data_image.cpp",
    "src/cpp/server/media/base64.cpp",
    "src/cpp/server/model_cache.cpp",
    "src/cpp/server/model_loader.cpp",
    "src/cpp/server/model_runtime.cpp",
    "src/cpp/server/model_metadata.cpp",
    "src/cpp/server/model_paths.cpp",
    "src/cpp/server/model_scanner.cpp",
    "src/cpp/tests/test_neo_package.cpp",
    "src/cpp/tests/test_text_sanitizer.cpp",
    "src/cpp/tests/test_markup_parser.cpp",
    "src/cpp/tests/test_http_parser.cpp",
    "src/cpp/tests/test_text_chunker.cpp",
    "src/cpp/tests/test_resource_policy.cpp",
    "src/cpp/tests/test_tts_request.cpp",
    "src/cpp/tests/test_sentence_splitter.cpp",
    "src/cpp/tests/test_model_registry.cpp",
    "src/cpp/tests/test_text_builtins.cpp",
    "src/cpp/text/builtin_matchers.cpp",
    "src/cpp/text/builtin_renderers.cpp",
    "src/cpp/text/protected_segments.cpp",]

EXPECTED_CONTEXT = {
    "README.md",
    "01-contexto-inicial-publico.md",
    "02-arquitectura-core-cpp.md",
    "03-servidor-api-tts.md",
    "04-normalizacion-texto.md",
    "05-paquetes-neo.md",
    "06-build-workflow.md",
    "07-pruebas-pendientes.md",
    "08-refactor-paquetes-neo.md",
    "09-restauracion-workflow-release.md",
    "10-refactor-modos-app-core.md",
    "11-refactor-cli-args.md",
    "12-api-publica-pruebas-scheduler.md",
    "13-refactor-sanitizer-api.md",
    "14-correccion-build-loadvoice.md",
    "15-refactor-markup-tts.md",
    "16-refactor-rutas-http.md",
    "17-refactor-http-parser.md",
    "18-refactor-registro-modelos.md",
    "19-refactor-scheduler-jobs.md",
    "20-smoke-binario-final.md",
    "21-refactor-cache-modelos.md",
    "22-refactor-text-chunker.md",
    "23-refactor-hardware-policy.md",
    "24-correccion-concurrencia-espeak.md",
    "25-refactor-pipeline-sintesis.md",
    "26-refactor-wav-utils-docs.md",
    "27-refactor-tts-route-request.md",
    "28-correccion-build-data-image-route.md",
    "29-refactor-sentence-splitter-model-tests.md",
    "30-cierre-refactor-cli-platform-docs.md",
}


def fail(message: str) -> None:
    raise SystemExit(f"ERROR: {message}")


def line_count(relative: str) -> int:
    return len((ROOT / relative).read_text(encoding="utf-8").splitlines())


def main() -> None:
    for relative in REQUIRED_FILES:
        if not (ROOT / relative).exists():
            fail(f"falta archivo requerido: {relative}")

    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    for source in REQUIRED_CMAKE_SOURCES:
        if source not in cmake:
            fail(f"CMakeLists.txt no referencia {source}")

    if "${CMAKE_CURRENT_SOURCE_DIR}/src/cpp" not in cmake:
        fail("CMakeLists.txt no agrega src/cpp como include dir")

    max_lines = {
        "src/cpp/piper.cpp": 80,
        "src/cpp/piper.hpp": 20,
        "src/cpp/neo_model.cpp": 140,
        "src/cpp/app/cli_args.cpp": 280,
        "src/cpp/app/hardware.cpp": 80,
        "src/cpp/app/synthesis_mode.cpp": 130,
        "src/cpp/app/platform.cpp": 20,
        "src/cpp/text/builtin_normalizer.cpp": 180,
        "src/cpp/core/text_chunker.cpp": 80,
        "src/cpp/core/sentence_splitter.cpp": 80,
        "src/cpp/core/wav_stream_writer.cpp": 120,
        "src/cpp/server/utils.cpp": 120,
        "src/cpp/server/tts_scheduler.cpp": 260,
        "src/cpp/server/model_cache.cpp": 150,
        "src/cpp/server/text_sanitizer.cpp": 120,
        "src/cpp/server/markup_tts.cpp": 260,
        "src/cpp/server/request_handler.cpp": 120,
        "src/cpp/server/routes/tts_routes.cpp": 180,
    }
    for relative, max_allowed in max_lines.items():
        count = line_count(relative)
        if count > max_allowed:
            fail(f"{relative} volvió a concentrar demasiada lógica: {count} líneas")


    platform_console_cpp = (ROOT / "src/cpp/app/platform_console.cpp").read_text(encoding="utf-8")
    platform_paths_cpp = (ROOT / "src/cpp/app/platform_paths.cpp").read_text(encoding="utf-8")
    if "SetConsoleOutputCP" not in platform_console_cpp or "SetConsoleCP" not in platform_console_cpp:
        fail("platform_console.cpp debe concentrar la configuracion UTF-8 de consola Windows")
    if "resolveExecutablePath" not in platform_paths_cpp or "GetModuleFileNameW" not in platform_paths_cpp:
        fail("platform_paths.cpp debe concentrar la resolucion de ruta del ejecutable")

    smoke_binary = (ROOT / "script/smoke-piper-binary.py").read_text(encoding="utf-8")
    for expected in ["--input_file", "--json-input", "--output_dir", "--output_raw", "--skip-cli-raw"]:
        if expected not in smoke_binary:
            fail(f"smoke-piper-binary.py debe probar o exponer {expected}")

    piper_header = (ROOT / "src/cpp/piper.hpp").read_text(encoding="utf-8")
    if '#include "piper/api.hpp"' not in piper_header:
        fail("src/cpp/piper.hpp debe quedar como fachada pública mínima")

    piper_api = (ROOT / "src/cpp/piper/api.hpp").read_text(encoding="utf-8")
    voice_loader = (ROOT / "src/cpp/core/voice_loader.cpp").read_text(encoding="utf-8")
    if "const std::optional<SpeakerId> &speakerId" not in piper_api:
        fail("loadVoice debe recibir speakerId como const reference")
    if "const std::optional<SpeakerId> &speakerId" not in voice_loader:
        fail("voice_loader.cpp debe mantener la firma const de speakerId")

    tts_scheduler_cpp = (ROOT / "src/cpp/server/tts_scheduler.cpp").read_text(encoding="utf-8")
    if "json resourcePolicyJson" in tts_scheduler_cpp or "json metricsJson" in tts_scheduler_cpp:
        fail("tts_scheduler.cpp no debe contener reportes JSON de métricas")
    if "std::array<char, 64 * 1024>" in tts_scheduler_cpp:
        fail("tts_scheduler.cpp no debe ensamblar WAV por buffers")

    phonemizer_cpp = (ROOT / "src/cpp/core/pipeline/phonemizer.cpp").read_text(encoding="utf-8")
    text_processing_cpp = (ROOT / "src/cpp/core/pipeline/text_processing.cpp").read_text(encoding="utf-8")
    if "globalPhonemizeMutex" not in phonemizer_cpp or "phonemize_eSpeak" not in phonemizer_cpp:
        fail("phonemizer.cpp debe proteger la fonemización eSpeak concurrente")
    if "globalTashkeelMutex" not in text_processing_cpp or "tashkeel_run" not in text_processing_cpp:
        fail("text_processing.cpp debe proteger libtashkeel concurrente")

    model_cache_cpp = (ROOT / "src/cpp/server/model_cache.cpp").read_text(encoding="utf-8")
    if "runtime->loadingSlots > 0" not in model_cache_cpp:
        fail("model_cache.cpp debe evitar cargas iniciales concurrentes del mismo modelo")

    http_cpp = (ROOT / "src/cpp/server/http.cpp").read_text(encoding="utf-8")
    http_url_cpp = (ROOT / "src/cpp/server/http/url.cpp").read_text(encoding="utf-8")
    utils_cpp = (ROOT / "src/cpp/server/utils.cpp").read_text(encoding="utf-8")
    if "ParsedTarget parseTarget" in utils_cpp or "std::string urlDecode" in utils_cpp:
        fail("utils.cpp no debe definir funciones HTTP")
    if "ParsedTarget parseTarget" in http_cpp or "std::string urlDecode" in http_cpp:
        fail("http.cpp debe quedarse como parser de request")
    if http_url_cpp.count("ParsedTarget parseTarget") != 1 or http_url_cpp.count("std::string urlDecode") != 1:
        fail("server/http/url.cpp debe conservar una sola definición de parseTarget y urlDecode")

    if "decodeBase64" in utils_cpp or "parseDataImage" in utils_cpp:
        fail("utils.cpp no debe conservar base64 ni data image")
    model_routes_cpp = (ROOT / "src/cpp/server/routes/model_routes.cpp").read_text(encoding="utf-8")
    if "parseDataImage" in model_routes_cpp and '#include "server/media/data_image.hpp"' not in model_routes_cpp:
        fail("model_routes.cpp usa parseDataImage pero no incluye server/media/data_image.hpp")

    for source in ["src/cpp/server/media/base64.cpp", "src/cpp/server/media/data_image.cpp"]:
        if source not in cmake:
            fail(f"CMakeLists.txt no referencia {source}")

    if '#include "types.hpp"' in (ROOT / "src/cpp/server/utils.hpp").read_text(encoding="utf-8"):
        fail("utils.hpp no debe incluir server/types.hpp")


    for removed in ["src/cpp/text/spanish_numbers.cpp", "src/cpp/text/spanish_numbers.hpp"]:
        if (ROOT / removed).exists():
            fail(f"normalizador numerico eliminado no debe existir: {removed}")
    if "spanish_numbers.cpp" in cmake or "spanish_numbers.hpp" in cmake:
        fail("CMakeLists.txt no debe referenciar spanish_numbers")

    if (ROOT / "apps").exists():
        fail("apps/ no debe existir en el repo público del motor Piper Neo")

    for workflow in [".github/workflows/build.yml", ".github/workflows/build-release.yml"]:
        if not (ROOT / workflow).exists():
            fail(f"falta workflow requerido: {workflow}")
    if (ROOT / ".github/workflows/main.yml").exists():
        fail(".github/workflows/main.yml fue reemplazado por build.yml y build-release.yml")

    build_workflow = (ROOT / ".github/workflows/build.yml").read_text(encoding="utf-8")
    release_workflow = (ROOT / ".github/workflows/build-release.yml").read_text(encoding="utf-8")
    if "push:" not in build_workflow or "branches:" not in build_workflow or "- main" not in build_workflow:
        fail("build.yml debe validar push hacia main")
    if "gh release create" in build_workflow or "gh release upload" in build_workflow:
        fail("build.yml no debe crear releases")
    if "workflow_dispatch:" not in release_workflow or "push:" in release_workflow:
        fail("build-release.yml debe ser manual y no automático")
    if "gh release create" not in release_workflow or "gh release upload" not in release_workflow:
        fail("build-release.yml debe crear o actualizar GitHub Releases")

    context_files = {path.name for path in (ROOT / "contexto").glob("*.md")}
    if context_files != EXPECTED_CONTEXT:
        fail(f"contexto/ no coincide con el set limpio esperado: {sorted(context_files)}")

    print("OK estructura core Piper")


if __name__ == "__main__":
    main()
