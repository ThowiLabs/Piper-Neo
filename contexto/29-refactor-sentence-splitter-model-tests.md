# Fecha

2026-06-19

# Objetivo

Continuar el endurecimiento final del core Piper Neo después de validar el smoke real en Windows, separando el detector de límites de oración y agregando pruebas unitarias para el registro de modelos sin depender de modelos ONNX reales.

# Decisiones tomadas

- Mantener `splitTextIntoExplicitSentenceChunks()` como API pública estable.
- Mover la detección de límites de oración a `core/sentence/boundary_detector.*`.
- Agregar pruebas específicas para abreviaturas, decimales, comillas, puntos suspensivos, saltos de línea y signos de pregunta españoles.
- Agregar pruebas de `ModelRegistry`/`scanModels` con archivos falsos `.onnx`, `.json` y `.neo`.
- No tocar `script/build-windows.py` ni workflows.

# Arquitectura actual

- `sentence_splitter.cpp` queda como orquestador de chunks explícitos.
- `sentence/boundary_detector.*` concentra reglas de puntuación, UTF-8, abreviaturas y saltos de línea.
- `test_sentence_splitter.cpp` valida comportamiento de segmentación sin cargar ONNX.
- `test_model_registry.cpp` valida escaneo, metadata JSON, resolución por nombre/stem, fallback a modelo activo y rechazo de nombres inseguros.

# Librerías usadas

- C++17 estándar.
- `std::filesystem` para pruebas temporales de modelos.
- `nlohmann::json` ya vendorizado por el proyecto.

# Archivos importantes modificados

- `src/cpp/core/sentence_splitter.cpp`
- `src/cpp/core/sentence/boundary_detector.hpp`
- `src/cpp/core/sentence/boundary_detector.cpp`
- `src/cpp/tests/test_sentence_splitter.cpp`
- `src/cpp/tests/test_model_registry.cpp`
- `CMakeLists.txt`
- `script/smoke-project-structure.py`

# Problemas encontrados

- El splitter de oraciones seguía siendo un archivo mediano con reglas mezcladas y sin prueba dedicada.
- El registro de modelos no tenía prueba fake que cubriera escaneo/resolución sin usar modelos reales.
- `CMakeLists.txt` tenía una referencia duplicada a `wav_stream_writer.cpp` en `test_piper`.

# Soluciones implementadas

- Se movieron reglas internas a `boundary_detector.*`.
- Se agregó `test_sentence_splitter`.
- Se agregó `test_model_registry`.
- Se eliminó la duplicación de `wav_stream_writer.cpp` dentro de `test_piper`.
- Se actualizó el smoke estructural para exigir los nuevos módulos y pruebas.

# Pendientes

- Probar build real en Windows.
- Ejecutar smoke del binario con modelos reales después de esta entrega.
- Agregar pruebas de integración HTTP más profundas si se desea validar rutas con servidor real desde CTest.

# Próximos pasos

- Revisar `builtin_normalizer.cpp` y `spanish_numbers.cpp` solo si se agregan pruebas específicas de normalización.
- Revisar `synthesis_mode.cpp` si se quiere separar aún más el CLI interactivo, aunque no es urgente.
