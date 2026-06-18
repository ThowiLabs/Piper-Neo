# Fecha

18 de junio de 2026

# Objetivo

Documentar el build local y el workflow público de GitHub para Piper Neo standalone.

# Decisiones tomadas

- El script local `script/build-windows.py` se conserva sin cambios.
- `.github/workflows/build.yml` se llama `Build Piper Neo` y valida automáticamente `push`/pull request hacia `main`.
- `.github/workflows/build-release.yml` se llama `Build Release Piper Neo` y solo corre con `workflow_dispatch`.
- El build automático no crea releases.
- El build-release manual crea o actualiza GitHub Releases y adjunta `piper_windows_amd64.zip`.
- El checkout del release usa explícitamente la rama `main`.
- Todos los workflows apuntan a `main` cuando necesitan una rama objetivo.

# Arquitectura actual

```text
script/build-windows.py          build local Windows usado para pruebas manuales
.github/workflows/build.yml           build automático en main, sin release
.github/workflows/build-release.yml   build-release manual con GitHub Releases
CMakeLists.txt                   build C++ del motor Piper Neo
Dockerfile.cli                   imagen CLI/servidor Linux
docker-compose-cli.yml           compose para servidor local
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

- `.github/workflows/build.yml`
- `.github/workflows/build-release.yml`
- `.gitignore`
- `CMakeLists.txt`
- `script/build-windows.py`

# Problemas encontrados

- El workflow anterior tenía nombre genérico `release`, por eso no se veía como build claro en GitHub Actions.
- Al no tener `push` sobre `main`, no quedaba como validación automática del commit inicial.
- GitHub puede mostrar el mensaje del commit como nombre del run si el workflow no define un nombre suficientemente claro.

# Soluciones implementadas

- `Build Piper Neo` mantiene el build automático y publica solo artefactos temporales de workflow.
- `Build Release Piper Neo` restaura el comportamiento original de creación/actualización de releases.
- El build-release manual resuelve versión numérica, crea o actualiza el release y adjunta `piper_windows_amd64.zip`.
- Se elimina la mezcla de release dentro del workflow automático.

# Pendientes

- Probar `Build Piper Neo` en el siguiente push a `main`.
- Probar `Build Release Piper Neo` manualmente desde Actions para confirmar que crea o actualiza el release.
- Confirmar que `vcpkg install zstd:x64-windows` funciona correctamente en `windows-latest`.

# Próximos pasos

Subir esta corrección a `main`, validar el workflow automático `Build Piper Neo` y ejecutar manualmente `Build Release Piper Neo` cuando se quiera publicar una versión descargable.
