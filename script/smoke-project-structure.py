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
        fail("CMakeLists.txt no agrega src/cpp como include dir; los modulos core no podran incluir piper.hpp")

    piper_lines = (ROOT / "src/cpp/piper.cpp").read_text(encoding="utf-8").splitlines()
    if len(piper_lines) > 80:
        fail(f"src/cpp/piper.cpp volvió a crecer demasiado: {len(piper_lines)} líneas")


    http_cpp = (ROOT / "src/cpp/server/http.cpp").read_text(encoding="utf-8")
    utils_cpp = (ROOT / "src/cpp/server/utils.cpp").read_text(encoding="utf-8")
    if "ParsedTarget parseTarget" in utils_cpp or "std::string urlDecode" in utils_cpp:
        fail("utils.cpp vuelve a definir funciones HTTP que deben vivir solo en http.cpp")
    if http_cpp.count("ParsedTarget parseTarget") != 1 or http_cpp.count("std::string urlDecode") != 1:
        fail("http.cpp debe conservar una sola definicion de parseTarget y urlDecode")


    if (ROOT / "apps").exists():
        fail("apps/ no debe existir en el repo público del motor Piper Neo")

    context_files = {path.name for path in (ROOT / "contexto").glob("*.md")}
    if context_files != EXPECTED_CONTEXT:
        fail(f"contexto/ no coincide con el set limpio esperado: {sorted(context_files)}")

    print("OK estructura core Piper")


if __name__ == "__main__":
    main()
