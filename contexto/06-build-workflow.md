# Fecha

18 de junio de 2026

# Objetivo

Documentar el build local y el workflow público de GitHub para Piper Neo standalone.

# Decisiones tomadas

- El script local `script/build-windows.py` se conserva sin cambios.
- El workflow `.github/workflows/main.yml` se llama ahora `Build Piper Neo` para que GitHub Actions lo muestre claramente como build.
- El workflow corre automáticamente en `push` a `main`.
- El workflow también permite ejecución manual con `workflow_dispatch` para publicar releases.
- El job de release solo corre cuando el workflow se ejecuta manualmente.
- El checkout usa explícitamente la rama `main`.
- No se usa `master` en el workflow.

# Arquitectura actual

```text
script/build-windows.py          build local Windows usado para pruebas manuales
.github/workflows/main.yml       build automático en main y release manual
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

- `.github/workflows/main.yml`
- `.gitignore`
- `CMakeLists.txt`
- `script/build-windows.py`

# Problemas encontrados

- El workflow anterior tenía nombre genérico `release`, por eso no se veía como build claro en GitHub Actions.
- Al no tener `push` sobre `main`, no quedaba como validación automática del commit inicial.
- GitHub puede mostrar el mensaje del commit como nombre del run si el workflow no define un nombre suficientemente claro.

# Soluciones implementadas

- El workflow ahora se llama `Build Piper Neo`.
- El `run-name` ahora muestra `Build Piper Neo #<número> · <rama>`.
- Se agregó trigger automático para `push` a `main`.
- El release se mantiene dentro del mismo workflow, pero condicionado a `workflow_dispatch`.
- El workflow publica artefacto `piper_windows_amd64.zip` en cada build.
- El release manual adjunta `piper_windows_amd64.zip` a una versión numérica.

# Pendientes

- Probar el workflow en GitHub después del siguiente push a `main`.
- Si GitHub muestra una X, abrir el run y revisar el primer step fallido real.
- Confirmar que `vcpkg install zstd:x64-windows` funciona correctamente en `windows-latest`.

# Próximos pasos

Subir esta corrección a `main`, entrar en la pestaña Actions y validar que aparezca el workflow `Build Piper Neo` con el job `Build Windows amd64`.
