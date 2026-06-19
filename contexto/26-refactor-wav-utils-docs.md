# Fecha

19 de junio de 2026

# Objetivo

Separar el streaming WAV del core y limpiar utilidades multimedia del servidor para reducir archivos medianos y dejar documentación final más útil para un repositorio público.

# Decisiones tomadas

- Mantener `textToWavFile()` y `textToWavFileFromStream()` como API pública estable.
- Separar el manejo de headers WAV/parcheo de tamaño en `src/cpp/core/wav/wav_header_writer.*`.
- Separar la síntesis de chunks hacia stream en `src/cpp/core/wav/stream_chunks.*`.
- Mantener `src/cpp/core/wav_stream_writer.cpp` como orquestador de alto nivel.
- Extraer Base64 y data URI de imagen desde `server/utils.*` hacia `server/media/*`.
- Agregar documentación final de build Windows, API HTTP y formato `.neo`.

# Arquitectura actual

- `core/wav_stream_writer.cpp`: entrada pública para WAV normal y WAV desde stream.
- `core/wav/wav_header_writer.*`: header RIFF/WAVE temporal, parcheo final y validación de 4 GiB.
- `core/wav/stream_chunks.*`: división/síntesis de chunks hacia `std::ostream`.
- `server/media/base64.*`: decodificación Base64 estricta para payloads internos.
- `server/media/data_image.*`: parseo de `data:image/...;base64,...` usado por model cards.

# Librerías usadas

Solo C++17 y las librerías ya existentes del proyecto. No se agregaron dependencias nuevas.

# Archivos importantes modificados

- `src/cpp/core/wav_stream_writer.cpp`
- `src/cpp/core/wav/wav_header_writer.hpp`
- `src/cpp/core/wav/wav_header_writer.cpp`
- `src/cpp/core/wav/stream_chunks.hpp`
- `src/cpp/core/wav/stream_chunks.cpp`
- `src/cpp/server/utils.hpp`
- `src/cpp/server/utils.cpp`
- `src/cpp/server/media/base64.hpp`
- `src/cpp/server/media/base64.cpp`
- `src/cpp/server/media/data_image.hpp`
- `src/cpp/server/media/data_image.cpp`
- `src/cpp/server/routes/model_routes.cpp`
- `src/cpp/tests/test_http_parser.cpp`
- `docs/build-windows.md`
- `docs/server-api.md`
- `docs/neo-format.md`

# Problemas encontrados

`wav_stream_writer.cpp` todavía concentraba header WAV, escritura incremental, síntesis de chunks y lectura streaming. `utils.cpp` conservaba lógica específica de Base64/data image que no pertenecía a utilidades generales.

# Soluciones implementadas

Se separaron responsabilidades reales sin cambiar contratos públicos, sin tocar workflows y sin modificar `script/build-windows.py`. También se amplió `test_http_parser` para validar Base64 y data URI de imagen.

# Pendientes

- Probar build real Windows después de este refactor.
- Ejecutar `script/smoke-piper-binary.py` con modelos reales.
- Si se agregan más formatos de imagen, ampliar `server/media/data_image.cpp` y su test.

# Próximos pasos

Agregar pruebas/fakes para `model_cache` y `model_registry`, y revisar si `tts_routes.cpp` requiere una separación adicional de parsing de request TTS.
