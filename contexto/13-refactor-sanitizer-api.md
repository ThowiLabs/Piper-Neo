# Fecha

18 de junio de 2026

# Objetivo

Continuar la refactorización del servidor TTS separando el sanitizador de texto de la API en módulos pequeños y agregando una prueba funcional dedicada para evitar regresiones en URLs, correos, markup, markdown, código, emojis, UTF-8 y límites de longitud.

# Decisiones tomadas

- `server/text_sanitizer.cpp` queda como orquestador público de sanitización.
- El estado `TtsTextSanitizeResult` se mueve a `server/sanitize_result.hpp` para no depender de `server/types.hpp` ni arrastrar el core Piper completo.
- Las utilidades del servidor (`server/utils.hpp`) dejan de incluir `server/types.hpp`; ahora solo incluyen `json.hpp` cuando necesitan JSON.
- Las reglas internas de sanitización viven bajo `server/sanitize/`.
- URLs y correos se cuentan, pero no se reescriben a frases como `enlace a` o `correo electronico`; esa pronunciación sigue bajo control de `neo.text_normalization` por modelo.

# Arquitectura actual

- `server/text_sanitizer.hpp`: API pública de sanitización para endpoints TTS.
- `server/text_sanitizer.cpp`: orquestador de flujo y respuesta JSON.
- `server/sanitize_result.*`: resultado, métricas y warnings únicos.
- `server/sanitize/utf8_text.*`: UTF-8 estricto, normalización Unicode, emojis, repeticiones, whitespace y recorte por caracteres.
- `server/sanitize/content_filters.*`: HTML, BBCode, markdown, código, alta entropía, URLs y correos.
- `server/sanitize/risk_score.*`: cálculo de riesgo basado en warnings.
- `tests/test_text_sanitizer.cpp`: prueba funcional autocontenida del sanitizador.

# Librerías usadas

No se agregaron dependencias nuevas. Se mantiene C++17, `std::regex` y `nlohmann/json` vendorizado.

# Archivos importantes modificados

- `src/cpp/server/text_sanitizer.hpp`
- `src/cpp/server/text_sanitizer.cpp`
- `src/cpp/server/sanitize_result.hpp`
- `src/cpp/server/sanitize_result.cpp`
- `src/cpp/server/sanitize/utf8_text.hpp`
- `src/cpp/server/sanitize/utf8_text.cpp`
- `src/cpp/server/sanitize/content_filters.hpp`
- `src/cpp/server/sanitize/content_filters.cpp`
- `src/cpp/server/sanitize/risk_score.hpp`
- `src/cpp/server/sanitize/risk_score.cpp`
- `src/cpp/server/types.hpp`
- `src/cpp/server/utils.hpp`
- `src/cpp/tests/test_text_sanitizer.cpp`
- `CMakeLists.txt`
- `script/smoke-project-structure.py`
- `docs/core-architecture.md`
- `docs/auditoria-refactor-piper-neo.md`

# Problemas encontrados

- `text_sanitizer.cpp` concentraba decodificación UTF-8, normalización Unicode, stripping HTML/Markdown/BBCode, detección de código, alta entropía, conteo de correos, emojis, recorte de longitud y cálculo de riesgo.
- `TtsTextSanitizeResult` vivía en `server/types.hpp`, que incluye tipos del core Piper y genera acoplamiento innecesario para probar sanitización.
- `utils.hpp` incluía `types.hpp` solo para tener alias JSON, arrastrando dependencias pesadas a utilidades simples.

# Soluciones implementadas

- `text_sanitizer.cpp` bajó a un orquestador pequeño.
- Se separó el resultado de sanitización en `sanitize_result.*`.
- Se movieron reglas Unicode y UTF-8 a `sanitize/utf8_text.*`.
- Se movieron filtros de contenido a `sanitize/content_filters.*`.
- Se movió el cálculo de riesgo a `sanitize/risk_score.*`.
- Se agregó `test_text_sanitizer` con casos de texto plano, URLs, correos, HTML, markdown, código, emojis, texto largo e UTF-8 inválido.
- Se reforzó el smoke estructural para detectar si el sanitizador vuelve a crecer o si `utils.hpp` vuelve a acoplarse a `types.hpp`.

# Pendientes

- Agregar pruebas HTTP que usen el sanitizador a través de `/api/v1/tts`.
- Agregar pruebas de sanitizer con inputs reales de usuarios y textos largos mixtos.
- Revisar `markup_tts.cpp`, que aún mezcla parser, validación, render, silencios y mezcla WAV.

# Próximos pasos

El siguiente refactor recomendable es `server/markup_tts.cpp`, separándolo en parser, validador y renderer, con pruebas específicas para segmentos `<voice>`, pausas/silencios y combinaciones inválidas.
