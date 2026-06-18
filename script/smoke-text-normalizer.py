#!/usr/bin/env python3
"""Compila y ejecuta una prueba mínima de normalización de texto.

No forma parte del build principal. Sirve como smoke test rápido para validar que
la normalización por modelo siga respetando compatibilidad clásica, reemplazos
legacy y reglas builtin protegidas.
"""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

SOURCES = [
    ROOT / "src/cpp/text/builtin_normalizer.cpp",
    ROOT / "src/cpp/text/replacements.cpp",
    ROOT / "src/cpp/text/spanish_numbers.cpp",
    ROOT / "src/cpp/text/string_utils.cpp",
    ROOT / "src/cpp/text_normalizer.cpp",
]

CPP_SOURCE = r'''
#include <iostream>
#include <string>

#include "text_normalizer.hpp"

namespace {

void expectEqual(const std::string &name, const std::string &got,
                 const std::string &expected) {
  if (got != expected) {
    std::cerr << "FAIL " << name << "\nexpected: " << expected
              << "\ngot:      " << got << "\n";
    std::exit(1);
  }
  std::cout << "OK " << name << "\n";
}

piper::TextNormalizationConfig parseConfig(const std::string &jsonText) {
  piper::TextNormalizationConfig config;
  auto root = nlohmann::json::parse(jsonText);
  piper::parseTextNormalizationConfig(root, config);
  return config;
}

} // namespace

int main() {
  {
    auto config = parseConfig(R"json({"audio":{"sample_rate":22050}})json");
    expectEqual("json clasico sin cambios",
                piper::normalizeTextForSpeech("Visita https://youtube.com", config),
                "Visita https://youtube.com");
  }

  {
    auto config = parseConfig(R"json({"modelcard":{"replacements":[["YouTube","Yutub"]]}})json");
    expectEqual("legacy replacements",
                piper::normalizeTextForSpeech("YouTube y youtube", config),
                "Yutub y Yutub");
  }

  {
    auto config = parseConfig(R"json({"neo":{"text_normalization":{"enabled":true,"builtin":{"decimals":true,"currency":true,"urls":true},"replacements":[{"from":"GitHub","to":"Guit Jab","whole_word":true}]}}})json");
    expectEqual("builtins protegidos y reemplazos",
                piper::normalizeTextForSpeech("Paga $99.50 pesos en https://github.com y abre GitHub 3.5", config),
                "Paga 99 punto 50 pesos en github punto com y abre Guit Jab tres punto cinco");
  }

  return 0;
}
'''


def find_compiler() -> str | None:
    env = os.environ.get("CXX")
    if env:
        return env
    for candidate in ("c++", "g++", "clang++"):
        found = shutil.which(candidate)
        if found:
            return found
    return None


def main() -> int:
    compiler = find_compiler()
    if not compiler:
        print("No se encontró compilador C++ compatible. Define CXX o instala g++/clang++.", file=sys.stderr)
        return 2

    with tempfile.TemporaryDirectory(prefix="piper-neo-textnorm-") as temp_dir:
        temp = Path(temp_dir)
        smoke_cpp = temp / "smoke_text_normalizer.cpp"
        output = temp / ("smoke_text_normalizer.exe" if os.name == "nt" else "smoke_text_normalizer")
        smoke_cpp.write_text(CPP_SOURCE, encoding="utf-8")

        cmd = [
            compiler,
            "-std=c++17",
            "-I",
            str(ROOT / "src/cpp"),
            str(smoke_cpp),
            *map(str, SOURCES),
            "-o",
            str(output),
        ]
        print("Compilando smoke test de normalización...")
        subprocess.run(cmd, check=True, cwd=ROOT)
        print("Ejecutando smoke test de normalización...")
        subprocess.run([str(output)], check=True, cwd=ROOT)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
