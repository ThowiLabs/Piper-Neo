# Fecha
2026-06-19

# Objetivo
Separar `src/cpp/core/synthesis_pipeline.cpp` en etapas internas más pequeñas sin cambiar la API pública `textToAudio()` ni el comportamiento esperado de CLI/API.

# Decisiones tomadas
- Mantener `synthesis_pipeline.cpp` como orquestador principal del pipeline.
- Extraer normalización y diacritización a `core/pipeline/text_processing.*`.
- Extraer fonemización segura a `core/pipeline/phonemizer.*`.
- Extraer división por frases, conversión a IDs y síntesis por frases a `core/pipeline/phrase_synthesizer.*`.
- Mantener los locks de eSpeak y libtashkeel, pero movidos a los módulos donde realmente se usan.
- No cambiar `script/build-windows.py`, workflows ni contrato público de `piper.hpp`.

# Arquitectura actual
El pipeline queda dividido en etapas:

- `synthesis_pipeline.cpp`: orquestación de texto a audio, silencios explícitos, callbacks y acumulación de resultados.
- `pipeline/text_processing.*`: normalización de texto y diacritización con tashkeel protegida por mutex.
- `pipeline/phonemizer.*`: fonemización por eSpeak/codepoints con protección de eSpeak concurrente.
- `pipeline/phrase_synthesizer.*`: split por silencios de fonemas, conversión a IDs, inferencia por frase y logs de phonemes faltantes.

# Librerías usadas
- C++17 standard library.
- spdlog para logs.
- piper-phonemize para fonemización.
- libtashkeel cuando el modelo lo requiere.
- ONNX Runtime indirectamente mediante `core/model_runtime.*`.

# Archivos importantes modificados
- `src/cpp/core/synthesis_pipeline.cpp`
- `src/cpp/core/pipeline/text_processing.hpp`
- `src/cpp/core/pipeline/text_processing.cpp`
- `src/cpp/core/pipeline/phonemizer.hpp`
- `src/cpp/core/pipeline/phonemizer.cpp`
- `src/cpp/core/pipeline/phrase_synthesizer.hpp`
- `src/cpp/core/pipeline/phrase_synthesizer.cpp`
- `CMakeLists.txt`
- `script/smoke-project-structure.py`

# Problemas encontrados
- El pipeline seguía mezclando normalización, tashkeel, fonemización, conversión a IDs, silencios por fonema, inferencia ONNX y métricas.
- El smoke estructural todavía buscaba los locks de concurrencia dentro de `synthesis_pipeline.cpp`; fue actualizado para validar los módulos nuevos.

# Soluciones implementadas
- Se redujo `synthesis_pipeline.cpp` a un orquestador de flujo.
- Se movieron los locks de eSpeak y tashkeel a módulos especializados.
- Se actualizó CMake para incluir los módulos nuevos en el binario principal y en `test_piper`.
- Se actualizó el smoke estructural para detectar que los módulos nuevos existan y estén referenciados.

# Pendientes
- Probar audio real con modelos `.onnx` y `.neo` después del build Windows.
- Agregar pruebas de integración de pipeline con modelo real usando `smoke-piper-binary.py`.
- Evaluar si `sentence_splitter.cpp` necesita pruebas adicionales de puntuación compleja.

# Próximos pasos
- Ejecutar `py script\build-windows.py clean` y `py script\build-windows.py` en Windows.
- Ejecutar `python script\smoke-piper-binary.py --binary dist-winlibs\piper-neo-windows\piper.exe --models <ruta-modelos> --stress-api-requests 4`.
- Revisar si quedan refactors menores en `wav_stream_writer.cpp` o documentación final.
