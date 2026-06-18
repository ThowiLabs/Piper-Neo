
# Fecha

18 de junio de 2026

# Objetivo

Documentar la arquitectura actual del core C++ después del refactor modular.

# Decisiones tomadas

- `main.cpp` queda como entrada mínima.
- CLI y modos de ejecución viven en `src/cpp/app/`.
- El núcleo Piper puro vive en `src/cpp/core/`.
- El servidor se divide entre `server.cpp` y módulos bajo `src/cpp/server/`.
- `piper.hpp` se conserva como contrato público para no romper integración interna.

# Arquitectura actual

```text
src/cpp/main.cpp                 entrada mínima
src/cpp/app/                     CLI, configuración, entorno, hardware y stdout RAW
src/cpp/core/                    runtime Piper, voz, ONNX, chunking, síntesis y WAV
src/cpp/server.cpp               runtime de servidor y loop accept
src/cpp/server/                  HTTP, auth, routing, sanitización, modelos, cache, scheduler y WAV
src/cpp/text_normalizer.cpp      API pública de normalización
src/cpp/text/                    implementación interna de normalización
src/cpp/neo_model.cpp            fachada pública de paquetes .neo
src/cpp/neo/                      implementación interna de paquetes .neo
```

# Librerías usadas

- C++17.
- fmt.
- spdlog.
- piper-phonemize.
- espeak-ng.
- ONNX Runtime.
- zstd opcional para paquetes `.neo`.
- nlohmann/json como header vendorizado.

# Archivos importantes modificados

- `CMakeLists.txt`
- `src/cpp/main.cpp`
- `src/cpp/app/*`
- `src/cpp/core/*`
- `src/cpp/server.cpp`
- `src/cpp/server/*`
- `src/cpp/text_normalizer.cpp`
- `src/cpp/text/*`
- `src/cpp/neo_model.cpp`
- `src/cpp/neo/*`

# Problemas encontrados

- El core previo concentraba demasiada lógica en archivos grandes.
- Cambios pequeños podían afectar CLI, API, audio o build.
- Faltaban smoke checks para proteger el refactor estructural.

# Soluciones implementadas

- Se separó por responsabilidades reales.
- Se agregó `src/cpp` como include root del target CMake.
- Se dejó `src/cpp/piper.cpp` mínimo y el pipeline real bajo `src/cpp/core/`.
- Se agregaron smoke checks estructurales.

# Pendientes

- Agregar pruebas funcionales de `.neo` para exportación, inspección, extracción e imagen.
- Separar rutas HTTP si el API aumenta.
- Agregar pruebas C++ específicas para chunking, paquetes `.neo` y parser HTTP.

# Próximos pasos

Priorizar pruebas antes de mover más lógica delicada de síntesis.
