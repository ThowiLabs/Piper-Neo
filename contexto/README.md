# Fecha

18 de junio de 2026

# Objetivo

Índice del contexto técnico actual de Piper Neo para mantener el repo público sin arrastrar historial viejo, apps externas ni archivos residuales.

# Decisiones tomadas

- El repo público queda enfocado en el motor Piper Neo.
- La carpeta `apps/` fue eliminada.
- El contexto anterior fue reemplazado por archivos actuales y coherentes.
- `Build Piper Neo` compila automáticamente en `push`/pull request hacia `main` y no crea releases.
- `Build Release Piper Neo` es manual y crea/actualiza GitHub Releases con `piper_windows_amd64.zip`.
- Los modelos reales, outputs y builds locales quedan ignorados.

# Arquitectura actual

Leer en este orden:

1. `01-contexto-inicial-publico.md`
2. `02-arquitectura-core-cpp.md`
3. `03-servidor-api-tts.md`
4. `04-normalizacion-texto.md`
5. `05-paquetes-neo.md`
6. `06-build-workflow.md`
7. `07-pruebas-pendientes.md`
8. `08-refactor-paquetes-neo.md`
9. `09-restauracion-workflow-release.md`
10. `10-refactor-modos-app-core.md`
11. `11-refactor-cli-args.md`

# Librerías usadas

Las mismas del proyecto: C++17, fmt, spdlog, piper-phonemize, espeak-ng, ONNX Runtime, zstd opcional y nlohmann/json vendorizado.

# Archivos importantes modificados

- `README.md`
- `README.es.md`
- `.gitignore`
- `.github/workflows/build.yml`
- `.github/workflows/build-release.yml`
- `contexto/*`
- `src/cpp/neo/*`
- `src/cpp/app/*`
- `docs/text-preprocessing.md`

# Problemas encontrados

El contexto previo mezclaba refactors, errores de build ya corregidos y referencias a apps externas que no deben formar parte del repo público del motor.

# Soluciones implementadas

Se reinició `contexto/` como documentación de estado actual, no como historial de conversación.

# Pendientes

Mantener estos archivos actualizados cuando cambien arquitectura, build, API, normalización, paquetes `.neo` o pruebas. El subsistema `.neo` ya vive en `src/cpp/neo/`, `neo_model.cpp` es solo fachada pública, los modos CLI viven en módulos específicos bajo `src/cpp/app/` y los argumentos separan parser, ayuda y validación.

# Próximos pasos

Probar el build real de Windows después del refactor de CLI y agregar pruebas funcionales de empaquetado/inspección/extracción `.neo`.
