# Fecha

18 de junio de 2026

# Objetivo

Restaurar el flujo de publicación de releases para Piper Neo sin hacer releases automáticos en cada push.

# Decisiones tomadas

- Se separó el build automático del build-release manual.
- `Build Piper Neo` queda como workflow automático de validación en `main`.
- `Build Release Piper Neo` queda como workflow manual con `workflow_dispatch`.
- El release manual restaura el comportamiento original: compila Windows amd64, empaqueta `piper_windows_amd64.zip`, resuelve la versión numérica y crea o actualiza GitHub Releases.
- La rama objetivo para release sigue siendo `main`.
- La rama objetivo es `main`.

# Arquitectura actual

```text
.github/workflows/build.yml           build automático, sin release
.github/workflows/build-release.yml   build-release manual con GitHub Releases
script/build-windows.py               build local Windows conservado sin cambios
```

# Librerías usadas

- GitHub Actions.
- actions/checkout@v4.
- actions/upload-artifact@v4.
- actions/download-artifact@v4.
- GitHub CLI `gh` disponible en runners de GitHub.
- vcpkg para `zstd:x64-windows`.
- CMake.

# Archivos importantes modificados

- `.github/workflows/build.yml`
- `.github/workflows/build-release.yml`
- `.github/workflows/main.yml` eliminado
- `README.md`
- `README.es.md`
- `contexto/README.md`
- `contexto/06-build-workflow.md`

# Problemas encontrados

El flujo anterior validaba el build en `main`, pero al usuario le faltaba recuperar el comportamiento de release del workflow original. También era confuso tener build automático y release manual dentro del mismo archivo.

# Soluciones implementadas

- Se creó `build.yml` para validación automática y artefactos temporales.
- Se creó `build-release.yml` para publicación manual de releases.
- `Build Release Piper Neo` no corre automáticamente; solo aparece al usar `Run workflow`.
- El release manual usa `main`, resuelve la versión y sube `piper_windows_amd64.zip` al release correspondiente.

# Pendientes

- Ejecutar manualmente `Build Release Piper Neo` desde GitHub Actions para confirmar que crea o actualiza el release.
- Si se requiere Linux/ARM64 en el futuro, agregar matrices nuevas al workflow manual sin mezclarlo con el build automático.

# Próximos pasos

Usar `Build Piper Neo` para validar commits normales y `Build Release Piper Neo` solo cuando se quiera publicar una versión descargable en GitHub Releases.
