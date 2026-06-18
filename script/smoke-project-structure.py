#!/usr/bin/env python3
"""Smoke test estructural del core Piper.

No compila dependencias nativas. Verifica que el refactor del core C++ siga
referenciado en CMake y que contexto/ conserve la bitácora limpia del proyecto.
"""
from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

REQUIRED_FILES = [
    "src/cpp/piper.cpp",
    "src/cpp/piper/types.hpp",
    "src/cpp/piper/api.hpp",
    "src/cpp/core/model_runtime.cpp",
    "src/cpp/core/model_runtime.hpp",
    "src/cpp/core/piper_runtime.cpp",
    "src/cpp/core/sentence_splitter.hpp",
    "src/cpp/core/sentence_splitter.cpp",
    "src/cpp/core/synthesis_pipeline.cpp",
    "src/cpp/core/synthesis_utils.hpp",
    "src/cpp/core/synthesis_utils.cpp",
    "src/cpp/core/text_chunker.cpp",
    "src/cpp/core/voice_loader.cpp",
    "src/cpp/core/wav_stream_writer.cpp",
    "src/cpp/neo/binary_io.cpp",
    "src/cpp/neo/compression.cpp",
    "src/cpp/neo/file_utils.cpp",
    "src/cpp/neo/image_payload.cpp",
    "src/cpp/neo/package_reader.cpp",
    "src/cpp/neo/package_writer.cpp",
    "src/cpp/neo_model.cpp",
    "src/cpp/app/export_neo_mode.cpp",
    "src/cpp/app/export_neo_mode.hpp",
    "src/cpp/app/server_mode.cpp",
    "src/cpp/app/server_mode.hpp",
    "src/cpp/app/synthesis_mode.cpp",
    "src/cpp/app/synthesis_mode.hpp",
    "src/cpp/app/voice_runtime.cpp",
    "src/cpp/server/metrics_report.cpp",
    "src/cpp/server/jobs/chunked_wav.cpp",
    "src/cpp/tests/test_neo_package.cpp",
    "src/cpp/app/voice_runtime.hpp",
    "src/cpp/app/cli_validation.cpp",
    "src/cpp/app/cli_validation.hpp",
    "src/cpp/app/help_text.cpp",
    "src/cpp/app/help_text.hpp",
    "src/cpp/server/metrics_report.hpp",
    "src/cpp/server/metrics_report.cpp",
    "src/cpp/server/jobs/chunked_wav.hpp",
    "src/cpp/server/jobs/chunked_wav.cpp",
    "src/cpp/tests/test_neo_package.cpp",
    "src/cpp/server/sanitize_result.hpp",
    "src/cpp/server/sanitize_result.cpp",
    "src/cpp/server/sanitize/content_filters.hpp",
    "src/cpp/server/sanitize/content_filters.cpp",
    "src/cpp/server/sanitize/risk_score.hpp",
    "src/cpp/server/sanitize/risk_score.cpp",
    "src/cpp/server/sanitize/utf8_text.hpp",
    "src/cpp/server/sanitize/utf8_text.cpp",
    "src/cpp/tests/test_text_sanitizer.cpp",
]

REQUIRED_CMAKE_SOURCES = [
    "src/cpp/core/model_runtime.cpp",
    "src/cpp/core/piper_runtime.cpp",
    "src/cpp/core/sentence_splitter.cpp",
    "src/cpp/core/synthesis_pipeline.cpp",
    "src/cpp/core/synthesis_utils.cpp",
    "src/cpp/core/text_chunker.cpp",
    "src/cpp/core/voice_loader.cpp",
    "src/cpp/core/wav_stream_writer.cpp",
    "src/cpp/neo/binary_io.cpp",
    "src/cpp/neo/compression.cpp",
    "src/cpp/neo/file_utils.cpp",
    "src/cpp/neo/image_payload.cpp",
    "src/cpp/neo/package_reader.cpp",
    "src/cpp/neo/package_writer.cpp",
    "src/cpp/neo_model.cpp",
    "src/cpp/app/cli_validation.cpp",
    "src/cpp/app/help_text.cpp",
    "src/cpp/app/export_neo_mode.cpp",
    "src/cpp/app/server_mode.cpp",
    "src/cpp/app/synthesis_mode.cpp",
    "src/cpp/app/voice_runtime.cpp",
    "src/cpp/server/metrics_report.cpp",
    "src/cpp/server/jobs/chunked_wav.cpp",
    "src/cpp/tests/test_neo_package.cpp",
    "src/cpp/server/sanitize_result.cpp",
    "src/cpp/server/sanitize/content_filters.cpp",
    "src/cpp/server/sanitize/risk_score.cpp",
    "src/cpp/server/sanitize/utf8_text.cpp",
    "src/cpp/tests/test_text_sanitizer.cpp",
]

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
}


def fail(message: str) -> None:
    raise SystemExit(f"ERROR: {message}")


def main() -> None:
    for relative in REQUIRED_FILES:
        if not (ROOT / relative).exists():
            fail(f"falta archivo requerido: {relative}")

    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    for source in REQUIRED_CMAKE_SOURCES:
        if source not in cmake:
            fail(f"CMakeLists.txt no referencia {source}")

    if "${CMAKE_CURRENT_SOURCE_DIR}/src/cpp" not in cmake:
        fail("CMakeLists.txt no agrega src/cpp como include dir; los modulos core no podran incluir piper/api.hpp o piper/types.hpp")

    piper_lines = (ROOT / "src/cpp/piper.cpp").read_text(encoding="utf-8").splitlines()
    if len(piper_lines) > 80:
        fail(f"src/cpp/piper.cpp volvió a crecer demasiado: {len(piper_lines)} líneas")
    piper_header = (ROOT / "src/cpp/piper.hpp").read_text(encoding="utf-8")
    if '#include "piper/api.hpp"' not in piper_header or len(piper_header.splitlines()) > 20:
        fail("src/cpp/piper.hpp debe quedar como fachada publica minima")


    neo_model_lines = (ROOT / "src/cpp/neo_model.cpp").read_text(encoding="utf-8").splitlines()
    if len(neo_model_lines) > 140:
        fail(f"src/cpp/neo_model.cpp volvió a concentrar demasiada lógica: {len(neo_model_lines)} líneas")

    cli_args_lines = (ROOT / "src/cpp/app/cli_args.cpp").read_text(encoding="utf-8").splitlines()
    if len(cli_args_lines) > 280:
        fail(f"src/cpp/app/cli_args.cpp volvió a concentrar help/validación: {len(cli_args_lines)} líneas")

    for app_source in [
        "src/cpp/app/cli_validation.cpp",
        "src/cpp/app/help_text.cpp",
    ]:
        if app_source not in cmake:
            fail(f"CMakeLists.txt no referencia módulo app: {app_source}")

    for source in [
        "src/cpp/neo/binary_io.cpp",
        "src/cpp/neo/compression.cpp",
        "src/cpp/neo/file_utils.cpp",
        "src/cpp/neo/image_payload.cpp",
        "src/cpp/neo/package_reader.cpp",
        "src/cpp/neo/package_writer.cpp",
    ]:
        if source not in cmake:
            fail(f"CMakeLists.txt no referencia módulo .neo: {source}")

    for source in [
        "src/cpp/server/metrics_report.cpp",
        "src/cpp/server/jobs/chunked_wav.cpp",
        "src/cpp/tests/test_neo_package.cpp",
    ]:
        if source not in cmake:
            fail(f"CMakeLists.txt no referencia módulo servidor/pruebas: {source}")


    tts_scheduler_cpp = (ROOT / "src/cpp/server/tts_scheduler.cpp").read_text(encoding="utf-8")
    if "json resourcePolicyJson" in tts_scheduler_cpp or "json metricsJson" in tts_scheduler_cpp:
        fail("tts_scheduler.cpp no debe contener reportes JSON de métricas")
    if "std::array<char, 64 * 1024>" in tts_scheduler_cpp:
        fail("tts_scheduler.cpp no debe ensamblar WAV por buffers; usar jobs/chunked_wav.cpp")

    http_cpp = (ROOT / "src/cpp/server/http.cpp").read_text(encoding="utf-8")
    utils_cpp = (ROOT / "src/cpp/server/utils.cpp").read_text(encoding="utf-8")
    if "ParsedTarget parseTarget" in utils_cpp or "std::string urlDecode" in utils_cpp:
        fail("utils.cpp vuelve a definir funciones HTTP que deben vivir solo en http.cpp")
    if http_cpp.count("ParsedTarget parseTarget") != 1 or http_cpp.count("std::string urlDecode") != 1:
        fail("http.cpp debe conservar una sola definicion de parseTarget y urlDecode")



    text_sanitizer_lines = (ROOT / "src/cpp/server/text_sanitizer.cpp").read_text(encoding="utf-8").splitlines()
    if len(text_sanitizer_lines) > 120:
        fail(f"src/cpp/server/text_sanitizer.cpp volvió a crecer demasiado: {len(text_sanitizer_lines)} líneas")
    for source in [
        "src/cpp/server/sanitize_result.cpp",
        "src/cpp/server/sanitize/content_filters.cpp",
        "src/cpp/server/sanitize/risk_score.cpp",
        "src/cpp/server/sanitize/utf8_text.cpp",
        "src/cpp/tests/test_text_sanitizer.cpp",
    ]:
        if source not in cmake:
            fail(f"CMakeLists.txt no referencia módulo sanitizer: {source}")
    if '#include "types.hpp"' in (ROOT / "src/cpp/server/utils.hpp").read_text(encoding="utf-8"):
        fail("utils.hpp no debe incluir server/types.hpp; usar json.hpp para evitar acoplar utilidades al core Piper")


    piper_api = (ROOT / "src/cpp/piper/api.hpp").read_text(encoding="utf-8")
    voice_loader = (ROOT / "src/cpp/core/voice_loader.cpp").read_text(encoding="utf-8")
    if "const std::optional<SpeakerId> &speakerId" not in piper_api:
        fail("loadVoice debe recibir speakerId como const reference para aceptar RunConfig inmutable")
    if "const std::optional<SpeakerId> &speakerId" not in voice_loader:
        fail("voice_loader.cpp debe mantener la firma const de speakerId")

    if (ROOT / "apps").exists():
        fail("apps/ no debe existir en el repo público del motor Piper Neo")

    required_workflows = [
        ".github/workflows/build.yml",
        ".github/workflows/build-release.yml",
    ]
    for workflow in required_workflows:
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
        fail("build-release.yml debe ser manual y no automatico")
    if "gh release create" not in release_workflow or "gh release upload" not in release_workflow:
        fail("build-release.yml debe crear o actualizar GitHub Releases")

    context_files = {path.name for path in (ROOT / "contexto").glob("*.md")}
    if context_files != EXPECTED_CONTEXT:
        fail(f"contexto/ no coincide con el set limpio esperado: {sorted(context_files)}")

    print("OK estructura core Piper")


if __name__ == "__main__":
    main()
