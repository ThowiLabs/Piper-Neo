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
12. `12-api-publica-pruebas-scheduler.md`
13. `13-refactor-sanitizer-api.md`
14. `14-correccion-build-loadvoice.md`
15. `15-refactor-markup-tts.md`
16. `16-refactor-rutas-http.md`
17. `17-refactor-http-parser.md`
18. `18-refactor-registro-modelos.md`
19. `19-refactor-scheduler-jobs.md`
20. `20-smoke-binario-final.md`
21. `21-refactor-cache-modelos.md`
22. `22-refactor-text-chunker.md`
23. `23-refactor-hardware-policy.md`
24. `24-correccion-concurrencia-espeak.md`
25. `25-refactor-pipeline-sintesis.md`
26. `26-refactor-wav-utils-docs.md`
27. `27-refactor-tts-route-request.md`
28. `28-correccion-build-data-image-route.md`
29. `29-refactor-sentence-splitter-model-tests.md`

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
- `docs/build-windows.md`
- `docs/server-api.md`
- `docs/neo-format.md`

# Problemas encontrados

El contexto previo mezclaba refactors, errores de build ya corregidos y referencias a apps externas que no deben formar parte del repo público del motor.

# Soluciones implementadas

Se reinició `contexto/` como documentación de estado actual, no como historial de conversación.

# Pendientes

Mantener estos archivos actualizados cuando cambien arquitectura, build, API, normalización, paquetes `.neo` o pruebas. El subsistema `.neo` vive en `src/cpp/neo/`, `neo_model.cpp` es fachada pública, los modos CLI viven bajo `src/cpp/app/`, `piper.hpp` es fachada pública, el scheduler delega estado/WAV/chunks a módulos `server/jobs/`, el sanitizer vive en `server/sanitize/`, markup TTS vive en `server/markup/`, rutas HTTP viven en `server/routes/`, HTTP base vive en `server/http/`, cache de modelos se separa en runtime/loader, chunking de texto vive en `core/text/`, la política de recursos vive separada de detección de hardware, la fonemización eSpeak/tashkeel queda protegida con mutex de alcance corto, `model_cache` evita cargas iniciales concurrentes, el streaming WAV ahora separa header/parcheo de chunks de audio, `/api/v1/tts` separa request JSON/payload de respuesta de la coordinación de síntesis y `model_routes.cpp` usa explícitamente `server/media/data_image.hpp` para imágenes embebidas, el splitter explícito de oraciones delega reglas a `core/sentence/boundary_detector.*`, existen pruebas fake para `ModelRegistry`/`scanModels`, el modo CLI de síntesis separa entrada/JSON/salida/rutas y la normalización builtin separa matchers/renderers/segmentos protegidos con pruebas propias.

# Próximos pasos

Probar el build real de Windows y ejecutar `script/smoke-piper-binary.py --models <ruta>` para validar CLI, API, TTS y descarga WAV con modelos reales.



## Avance posterior: synthesis mode y builtins de texto

- `src/cpp/app/synthesis_mode.cpp`: reducido a orquestador del modo síntesis CLI.
- `src/cpp/app/synthesis_input.*`: entrada directa desde texto, archivo o stdin.
- `src/cpp/app/synthesis_json.*`: overrides por línea JSON.
- `src/cpp/app/synthesis_output.*`: salida WAV/stdout/raw y logging de resultado.
- `src/cpp/app/synthesis_paths.*`: nombres de salida por timestamp.
- `src/cpp/text/builtin_normalizer.cpp`: reducido a scanner de tokens builtin.
- `src/cpp/text/builtin_matchers.*`: regex y límites seguros.
- `src/cpp/text/builtin_renderers.*`: conversión hablada de URL, correo, versión, moneda y porcentaje.
- `src/cpp/text/protected_segments.*`: protección temporal para evitar que reemplazos personalizados rompan tokens builtin.
- `src/cpp/tests/test_text_builtins.cpp`: pruebas unitarias de números, URL, email, moneda, versiones, porcentajes y límites de decimales.
