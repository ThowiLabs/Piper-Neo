# Auditoría técnica actual de Piper Neo

## Alcance

Esta auditoría describe el estado del motor Piper Neo después de limpiar el repo público y eliminar `apps/`. El alcance actual es el motor: C++ CLI, servidor HTTP local, paquetes `.neo`, normalización, scripts de build/smoke, documentación y utilidades de entrenamiento/runtime heredadas de Piper.

## Hallazgos principales corregidos

### 1. `main.cpp` concentraba lógica de aplicación

Solución implementada: `main.cpp` quedó como entrada mínima y la lógica vive en `src/cpp/app/`.

### 2. `piper.cpp` concentraba runtime, voz, inferencia y WAV

Solución implementada: el core se movió a `src/cpp/core/` y `piper.cpp` conserva solo la versión pública.

### 3. `server.cpp` concentraba HTTP, modelos, scheduler y limpieza

Solución implementada: servidor dividido en módulos bajo `src/cpp/server/`.

### 4. `text_normalizer.cpp` mezclaba reglas y utilidades

Solución implementada: normalización dividida en `src/cpp/text/` con smoke test dedicado.

### 5. Build modular necesitaba include root claro

Solución implementada: `CMakeLists.txt` agrega `${CMAKE_CURRENT_SOURCE_DIR}/src/cpp` como include root para `piper` y `test_piper`.

### 6. Duplicados HTTP podían romper linker

Solución implementada: `parseTarget` y `urlDecode` quedaron centralizados en `server/http.cpp`.

### 7. Repo público contenía componentes fuera de alcance

Solución implementada: se eliminó `apps/`, se limpió `.gitignore`, se reescribió README y se reinició `contexto/` para el estado público actual.

### 8. `piper_app.cpp` mezclaba modos de ejecución

Solución implementada: la exportación `.neo`, carga de runtime/voz, modo servidor y síntesis CLI/stdin/JSON/RAW quedaron separados en módulos específicos bajo `src/cpp/app/`.

### 9. `cli_args.cpp` mezclaba ayuda, parsing y validación

Solución implementada: la ayuda CLI vive en `help_text.*`, la validación cruzada/rutas vive en `cli_validation.*` y `cli_args.cpp` queda enfocado en convertir flags a `RunConfig`.

### 10. `piper.hpp` seguía actuando como cabecera monolítica

Solución implementada: `piper.hpp` queda como fachada compatible; los tipos públicos viven en `piper/types.hpp` y las funciones públicas en `piper/api.hpp`.

### 11. `tts_scheduler.cpp` mezclaba cola, reportes, chunks y ensamblado WAV

Solución implementada: los reportes JSON viven en `server/metrics_report.*` y el ensamblado de chunks RAW a WAV vive en `server/jobs/chunked_wav.*`.

### 12. Faltaba prueba funcional mínima de `.neo`

Solución implementada: se agregó `test_neo_package`, que genera un `.neo` mínimo sin modelo ONNX real, lo inspecciona, lee su imagen y extrae modelo/config.

### 13. `text_sanitizer.cpp` mezclaba demasiadas reglas de entrada API

Solución implementada: el sanitizador se separó en `server/sanitize_result.*` y módulos bajo `server/sanitize/` para UTF-8/Unicode, filtros de contenido y cálculo de riesgo. También se agregó `test_text_sanitizer`.

## Deuda técnica restante

### Alta prioridad

1. `src/cpp/app/synthesis_mode.cpp`: agregar pruebas de stdin, JSON input, output WAV y RAW.
2. Pruebas CLI: validar combinaciones de flags, `--help`, `--version`, `--server` y export `.neo`.
3. `.neo`: ampliar prueba para export zstd real cuando zstd esté disponible.
4. Server: el smoke del binario ya valida health, modelos, TTS y descarga de archivos; falta prueba con socket simulado dentro de CTest si se requiere.

### Prioridad media

1. `src/cpp/server/tts_scheduler.cpp`: ya separa estado, ciclo de vida y worker de chunks; seguir separando solo si crece la concurrencia.
2. `src/cpp/server/request_handler.cpp`: ya separa rutas; falta prueba HTTP con sockets simulados.
3. `src/cpp/server/markup_tts.cpp`: ya separa parser/opciones/audio; falta prueba de integración HTTP/WAV real.
4. `src/cpp/server/model_registry.cpp`: cuenta con test fake de scanner/metadata/resolución; falta cache con ONNX real si se requiere.

### Prioridad baja

1. `src/cpp/core/synthesis_pipeline.cpp`: ya fue separado en etapas internas; falta validar audio real con más modelos.
2. `src/cpp/app/platform.cpp`: revisar si conviene separar consola Windows, paths y entorno.
3. `CMakeLists.txt`: ordenar secciones si crece más, evitando fragmentar el build sin necesidad.

## Pruebas recomendadas antes de más refactor

- `.neo`: ya existe prueba mínima sin compresión; falta validar export zstd/no-zstd con zstd real.
- Chunking: `test_text_chunker` cubre límites de oración, signos españoles, UTF-8 y palabras largas; falta ampliar con URLs, correos, decimales y abreviaturas reales.
- HTTP: GET, POST, query params, payload grande, headers malformados.
- Sanitizer: `test_text_sanitizer` ya cubre texto normal, código, URLs/correos, markup, markdown, emoji, input enorme e UTF-8 inválido; falta probar integración HTTP.
- Markup: `test_markup_parser` cubre `<model>`, `<silence>`, speakers y opciones request; falta integración con scheduler/WAV real.
- Audio real: `.onnx`, `.neo`, `--output_raw`, stdin largo y servidor local.

## Estado recomendado

El repo está listo como base pública del motor Piper Neo. El refactor grande de `.neo`, la separación de modos de aplicación, la limpieza de CLI, sanitizer y markup TTS ya fueron aplicados; el siguiente paso recomendable es agregar pruebas HTTP pequeñas y separar `http.cpp` si crece el parser/socket IO.

## Avance posterior: HTTP base y modelos

- `src/cpp/server/http.cpp`: dividido para separar parser de request, socket I/O, respuestas y URL/query helpers.
- `src/cpp/server/http_types.hpp`: tipos HTTP ligeros separados de `server/types.hpp`, evitando arrastrar `piper.hpp`/ONNX a pruebas simples.
- `src/cpp/server/model_registry.cpp`: reducido a cache/refresh/lookup.
- `src/cpp/server/model_metadata.cpp`: extrae construcción de JSON público de modelos.
- `src/cpp/server/model_scanner.cpp`: extrae escaneo de `.onnx`/`.neo`.
- `src/cpp/server/model_paths.cpp`: centraliza `modelKey()` para eliminar duplicación con `model_cache.cpp`.

Pendiente: pruebas funcionales más específicas para scanner/metadata/cache, scheduler bajo concurrencia y casos de audio real con varios modelos.


## Smoke del binario final

Se agregó `script/smoke-piper-binary.py` para probar un binario ya compilado con modelos reales: `--help`, `--version`, síntesis CLI, arranque de API, health/status/metrics, listado de modelos, validación negativa de TTS, generación TTS por API y descarga WAV.

## Corrección de concurrencia eSpeak/model cache

Se corrigió una regresión detectada con textos largos y varios modelos en modo API: varios chunks podían cargar réplicas del mismo modelo en paralelo y fonemizar simultáneamente con eSpeak-ng. En Windows esto podía producir `Bad data: es_dict length=0`.

La solución serializa solo la sección global de eSpeak/tashkeel y evita cargas iniciales concurrentes del mismo modelo. La inferencia ONNX y las réplicas siguen disponibles para paralelismo después de la fonemización. El smoke del binario final ahora permite stress concurrente con `--stress-api-requests`.


## Avance posterior: pipeline de síntesis

- `src/cpp/core/synthesis_pipeline.cpp`: reducido a orquestador de alto nivel.
- `src/cpp/core/pipeline/text_processing.*`: normalización de texto y diacritización tashkeel con lock de concurrencia.
- `src/cpp/core/pipeline/phonemizer.*`: fonemización eSpeak/codepoints con lock corto para evitar corrupción de diccionarios.
- `src/cpp/core/pipeline/phrase_synthesizer.*`: división por silencios de fonemas, conversión a IDs, inferencia por frases y logs de fonemas faltantes.

Pendiente: validar audio real con `.onnx`, `.neo`, textos largos, `--output_raw` y stress API concurrente después de compilar el binario Windows.

## Avance posterior: WAV streaming y media utils

- `src/cpp/core/wav_stream_writer.cpp`: reducido a orquestador de WAV normal y WAV desde stdin/stream.
- `src/cpp/core/wav/wav_header_writer.*`: extrae header RIFF/WAVE temporal, validación de límite 4 GiB y parcheo final.
- `src/cpp/core/wav/stream_chunks.*`: extrae síntesis de chunks y escritura incremental de audio.
- `src/cpp/server/utils.cpp`: deja de contener Base64/data URI.
- `src/cpp/server/media/base64.*`: contiene decodificación Base64.
- `src/cpp/server/media/data_image.*`: contiene parseo de imágenes `data:image/...;base64`.

Pendiente: agregar pruebas unitarias más profundas para WAV streaming sin depender de un modelo real, usando una interfaz de síntesis simulable si más adelante se justifica.

## Avance posterior: request TTS

- `src/cpp/server/routes/tts_routes.cpp`: deja de parsear directamente todos los campos JSON.
- `src/cpp/server/routes/tts_request.*`: concentra validación de `text`, `model`, `default_model`, `speaker_id`, aliases de opciones y límites de entrada.
- `src/cpp/server/routes/tts_payload.*`: concentra el payload estándar de éxito TTS.

Implementado: `test_tts_request.cpp` valida errores de request sin depender del servidor real.

## Avance posterior: sentence splitter y registro de modelos

- `src/cpp/core/sentence_splitter.cpp`: queda como orquestador de chunks explícitos.
- `src/cpp/core/sentence/boundary_detector.*`: concentra reglas de límites de oración, abreviaturas, decimales, comillas, puntos suspensivos, saltos de línea y UTF-8.
- `src/cpp/tests/test_sentence_splitter.cpp`: valida reglas de segmentación sin cargar modelos.
- `src/cpp/tests/test_model_registry.cpp`: valida scanner, metadata JSON, resolución por nombre/stem, fallback a modelo activo y rechazo de nombres inseguros usando archivos fake.
