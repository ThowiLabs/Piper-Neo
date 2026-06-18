# Fecha

18 de junio de 2026

# Objetivo

Continuar la refactorización del servidor TTS separando `markup_tts.cpp`, que todavía mezclaba parsing de etiquetas, validación de opciones, síntesis de segmentos, silencios y ensamblado WAV multi-voz.

# Decisiones tomadas

- `markup_tts.cpp` queda como renderer/orquestador de alto nivel para `synthesizeMarkupScript()`.
- El parser de markup vive bajo `server/markup/markup_parser.*`.
- La lectura de opciones flotantes desde JSON vive en `server/markup/request_options.*`.
- Las piezas de audio temporales y la escritura de segmentos WAV/JSON viven en `server/markup/audio_parts.*`.
- Se mantiene el contrato público de `markup_tts.hpp` para no romper `request_handler.cpp` ni endpoints existentes.
- Se agrega prueba dedicada de parser para validar tags `<model>`, `<silence>`, speaker por atributo y speaker por `modelo#id`.

# Arquitectura actual

- `server/markup_tts.hpp`: API pública del renderer markup TTS.
- `server/markup_tts.cpp`: flujo de síntesis multi-segmento y limpieza de temporales.
- `server/markup/markup_parser.*`: detección markup, atributos loose, modelos, speakers, silencios y segmentos.
- `server/markup/request_options.*`: lectura validada de opciones float desde requests JSON.
- `server/markup/audio_parts.*`: estructura temporal de audio, resampling, cálculo de bytes, escritura WAV y JSON de segmentos.
- `tests/test_markup_parser.cpp`: prueba funcional del parser y de opciones request.

# Librerías usadas

No se agregaron dependencias nuevas. Se mantiene C++17, `std::regex`, `std::filesystem` y `nlohmann/json` vendorizado.

# Archivos importantes modificados

- `src/cpp/server/markup_tts.hpp`
- `src/cpp/server/markup_tts.cpp`
- `src/cpp/server/markup/markup_parser.hpp`
- `src/cpp/server/markup/markup_parser.cpp`
- `src/cpp/server/markup/request_options.hpp`
- `src/cpp/server/markup/request_options.cpp`
- `src/cpp/server/markup/audio_parts.hpp`
- `src/cpp/server/markup/audio_parts.cpp`
- `src/cpp/tests/test_markup_parser.cpp`
- `CMakeLists.txt`
- `script/smoke-project-structure.py`
- `docs/core-architecture.md`
- `docs/auditoria-refactor-piper-neo.md`
- `README.md`
- `README.es.md`

# Problemas encontrados

- `markup_tts.cpp` concentraba parser, validación, render, WAV, JSON y limpieza de temporales.
- Había una llave sobrante en `markupVoiceSettingsJson()` dentro del archivo monolítico, riesgo que se elimina al mover esa lógica a `audio_parts.cpp`.
- No existía prueba específica para markup, por lo que cambios en `<silence>` o `<model>` podían romperse sin detección temprana.

# Soluciones implementadas

- `markup_tts.cpp` bajó de un archivo monolítico a un orquestador enfocado en síntesis multi-segmento.
- Se movió el parser a `markup_parser.*`.
- Se movió `requestFloatOption()` a `request_options.*`.
- Se movió la lógica de piezas de audio y JSON de segmentos a `audio_parts.*`.
- Se agregó `test_markup_parser` y se registró en CMake/CTest.
- Se reforzó `smoke-project-structure.py` para evitar que `markup_tts.cpp` vuelva a concentrar parser/render.

# Pendientes

- Agregar prueba de integración HTTP para markup TTS real.
- Agregar prueba con WAV de segmentos mockeados para validar timeline `start_ms/end_ms` sin requerir ONNX.
- Seguir separando `request_handler.cpp` si crecen las rutas HTTP.

# Próximos pasos

El siguiente refactor recomendable es `server/request_handler.cpp`, separando rutas HTTP (`health`, `models`, `tts`, `files`, `metrics`) para que el router no siga creciendo.
