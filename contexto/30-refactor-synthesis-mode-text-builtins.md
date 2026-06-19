# Fecha

19 de junio de 2026

# Objetivo

Reducir los últimos bloques medianos de la capa CLI y normalización de texto sin cambiar el contrato público del binario ni la API HTTP.

# Decisiones tomadas

- `runSynthesisMode()` conserva el mismo comportamiento público, pero deja de contener toda la lógica de entrada, JSON y salida.
- La normalización builtin se separa en renderers, matchers y segmentos protegidos para que URLs, correos, moneda, versiones, porcentajes y decimales sean testeables por separado.
- Se agrega una prueba C++ ligera para normalización builtin sin cargar ONNX, eSpeak ni piper-phonemize.
- Se restaura la presencia de `.github/workflows/build.yml` y `.github/workflows/build-release.yml` en el ZIP público porque el smoke estructural los requiere y forman parte del repo entregable.

# Arquitectura actual

`src/cpp/app/synthesis_mode.cpp` queda como orquestador de modo síntesis. La lógica auxiliar se divide en:

- `src/cpp/app/synthesis_input.*`: entrada directa desde `--text`, `--input_file` o stdin.
- `src/cpp/app/synthesis_json.*`: overrides por línea JSON (`output_file`, `speaker_id`, `speaker`).
- `src/cpp/app/synthesis_output.*`: escritura WAV/stdout/raw y logging de resultado.
- `src/cpp/app/synthesis_paths.*`: nombres temporales para salida por directorio.

`src/cpp/text/builtin_normalizer.cpp` queda como scanner de tokens. La lógica interna se divide en:

- `src/cpp/text/builtin_matchers.*`: regex y límites seguros.
- `src/cpp/text/builtin_renderers.*`: conversión hablada de URL, correo, versión, moneda y porcentaje.
- `src/cpp/text/protected_segments.*`: marcadores internos para proteger tokens normalizados antes de aplicar reemplazos personalizados.

# Librerías usadas

C++17, nlohmann/json vendorizado y la infraestructura existente de Piper Neo. Las pruebas nuevas no requieren dependencias nativas de audio.

# Archivos importantes modificados

- `CMakeLists.txt`
- `script/smoke-project-structure.py`
- `script/smoke-text-normalizer.py`
- `src/cpp/app/synthesis_mode.cpp`
- `src/cpp/text/builtin_normalizer.cpp`
- `src/cpp/tests/test_text_builtins.cpp`
- `contexto/README.md`

# Problemas encontrados

- `synthesis_mode.cpp` todavía mezclaba lectura de entrada, modo directo, JSON por línea, salida WAV/stdout/raw y logging.
- `builtin_normalizer.cpp` concentraba regex, renderizado hablado, marcadores protegidos y scanner principal.
- El ZIP base usado para continuar no traía `.github/workflows/`, aunque el smoke estructural los seguía esperando.

# Soluciones implementadas

- Se separó el modo síntesis CLI en módulos internos pequeños.
- Se separó la normalización builtin en matchers/renderers/protected segments.
- Se agregó `test_text_builtins.cpp` para validar números, versiones, emails, URLs, moneda, porcentajes, signos finales y límites seguros de decimales.
- Se actualizó `smoke-text-normalizer.py` para compilar los nuevos módulos de normalización.
- Se restauraron los workflows públicos `build.yml` y `build-release.yml`.

# Pendientes

- Probar build Windows real con este ZIP.
- Ejecutar `script/smoke-piper-binary.py` con modelos reales y stress concurrente.
- Agregar pruebas específicas de CLI JSON/stdin/output_raw usando el binario final.
- Revisar `platform.cpp` solo si crece o aparecen diferencias Windows/Linux.

# Próximos pasos

Ejecutar smoke estructural, normalización, tests CMake y después el smoke del binario final en Windows.
