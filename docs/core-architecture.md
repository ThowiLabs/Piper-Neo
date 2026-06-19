# Arquitectura del core C++ de Piper Neo

## Objetivo

El core C++ queda organizado por responsabilidades reales, manteniendo la CLI y el servidor HTTP en el mismo binario `piper`, pero evitando que archivos de entrada o runtime concentren lógica de dominio, parsing, seguridad, audio, HTTP y normalización.

## Principios aplicados

- Entrada del programa mínima.
- Configuración centralizada de ejecución.
- Separación por responsabilidades reales, no por capas vacías.
- Sin dependencias nuevas.
- Sin cambio de contrato público de CLI ni endpoints HTTP.
- Sin modificar scripts locales de build/packaging.
- Pruebas smoke pequeñas para lógica crítica y estructura.

## Estructura principal

```text
src/cpp/main.cpp
src/cpp/app/
src/cpp/server.cpp
src/cpp/server/
src/cpp/piper.hpp
src/cpp/piper/
src/cpp/piper.cpp
src/cpp/core/
src/cpp/text_normalizer.cpp
src/cpp/text/
src/cpp/neo_model.cpp
src/cpp/neo/
```

## `main.cpp`

Responsabilidad única:

- Configurar consola UTF-8 en Windows.
- Ejecutar `piper_app::piperMain`.
- Capturar errores fatales de entrada.

## `src/cpp/app/`

Contiene la capa de aplicación CLI:

```text
run_config.hpp          Configuración de ejecución.
cli_args.*              Parser de argumentos y asignación directa a RunConfig.
help_text.*             Ayuda CLI y opciones visibles para usuario.
cli_validation.*        Validación cruzada, rutas, modo servidor y resolución de modelo/config.
hardware.*              Orquestador de detección, política de recursos y logging.
hardware_probe.*        Detección de CPU, memoria y límites cgroup.
resource_limits.*       Límites puros de memoria temporal y réplicas.
resource_policy.*       Perfiles auto/eco/balanced/fast/max y clamps de concurrencia.
env.*                   Resolución de token desde argumento, entorno o .env.
platform.*              Detalles del ejecutable y consola por plataforma.
raw_audio_output.*      Escritura progresiva de audio RAW a stdout.
export_neo_mode.*       Modo de exportación de paquetes `.neo`.
voice_runtime.*         Preparación de voz, eSpeak, tashkeel y overrides de síntesis.
server_mode.*           Conversión de RunConfig a ServerOptions y ejecución HTTP.
synthesis_mode.*        Síntesis CLI por stdin, archivo, JSON, WAV y RAW.
piper_app.*             Orquestador mínimo y selector de modo.
```

## `src/cpp/core/`

Contiene el núcleo Piper puro: carga de voz, runtime, normalización previa, fonemización, inferencia, silencios y salida WAV.

```text
piper_runtime.cpp       initialize/terminate de eSpeak y tashkeel.
voice_loader.cpp        Parseo JSON de voz, speaker, normalización y carga de modelo.
model_runtime.hpp       Contrato interno de carga ONNX e inferencia.
model_runtime.cpp       Sesión ONNX Runtime y conversión float -> int16.
synthesis_pipeline.cpp  Normalización, tashkeel, fonemización, phoneme ids, frases y silencios.
wav_stream_writer.cpp   WAV progresivo, header seekable y lectura larga desde stdin.
synthesis_utils.hpp     Utilidades internas compartidas.
synthesis_utils.cpp     Cancelación, preview UTF-8, BOM/whitespace y silencios.
text_chunker.hpp        Contrato ligero de particionado de texto.
text_chunker.cpp        Orquestador de chunks para textos largos.
core/text/utf8_utils.*  Helpers de límites UTF-8 y whitespace.
core/text/chunk_rules.* Reglas de corte por oración, párrafo, signos españoles y hard limit.
sentence_splitter.hpp   Contrato interno para pausas explícitas.
sentence_splitter.cpp   Orquestador de chunks explícitos y silencios entre oraciones.
core/sentence/boundary_detector.* Reglas de oración: puntuación, comillas, decimales, abreviaturas, UTF-8 y saltos de línea.
```

`src/cpp/piper.cpp` queda mínimo y conserva solo `getVersion()`. El contrato público mantiene compatibilidad por `piper.hpp`, pero internamente se separó en:

```text
piper.hpp           Fachada pública compatible.
piper/types.hpp     Tipos públicos: configuración, voz, síntesis y sesión.
piper/api.hpp       Funciones públicas: runtime, carga de voz, chunking y síntesis.
```

Los módulos internos nuevos deben incluir `piper/types.hpp` o `piper/api.hpp` directamente para evitar depender de una cabecera monolítica.

## `src/cpp/server.cpp`

Queda como runtime del servidor:

- Inicializa carpetas y limpieza.
- Inicializa sockets.
- Calcula política de recursos efectiva.
- Crea `ModelRegistry`, `ModelCache`, `ServerMetrics` y `FairTtsScheduler`.
- Acepta conexiones y delega cada cliente a `server/request_handler.*`.

## `src/cpp/server/`

Módulos internos del servidor:

```text
types.hpp               Tipos compartidos del servidor.
utils.*                 Fechas, strings, nombres seguros, JSON, base64 y data images.
http.*                  Socket portable, request parser, rutas, respuestas HTTP y archivos.
auth.*                  Bearer token y X-API-Token.
request_handler.*       Routing HTTP de una conexión y validaciones por endpoint.
responses.*             Respuestas JSON y mapeo de errores.
text_sanitizer.*        Orquestador público de limpieza segura antes de síntesis API.
sanitize_result.*       Resultado y warnings únicos de sanitización.
sanitize/utf8_text.*    UTF-8 estricto, Unicode, emojis, whitespace y recorte.
sanitize/content_filters.* HTML, BBCode, markdown, código, alta entropía, URLs y correos.
sanitize/risk_score.*   Cálculo de riesgo por warnings de sanitización.
model_registry.*        Cache temporal, refresh y búsqueda de modelos.
model_metadata.*        Metadata JSON, modelcard, imágenes y campos técnicos.
model_scanner.*         Escaneo de .onnx/.neo y selección de primer modelo usable.
model_paths.*           Claves canónicas de modelo compartidas por registry/cache.
model_cache.*           Orquestador de cache de modelos y checkout de leases.
model_runtime.*         Slots, runtime y VoiceLease con RAII para liberar voces.
model_loader.*          Carga real de .onnx/.neo y creación de piper::Voice.
tts_scheduler.*         Cola justa y coordinación de workers de síntesis.
metrics_report.*        Reportes JSON de política de recursos y métricas.
jobs/job_state.*        Estado compartido de jobs y work items.
jobs/job_lifecycle.*    Creación, cierre, limpieza y resultado de jobs.
jobs/chunk_worker.*     Síntesis de chunks con overrides temporales de voz.
jobs/chunked_wav.*      Ensamblado de WAV desde chunks RAW temporales.
markup_tts.*            Orquestador de síntesis markup TTS multi-segmento.
markup/markup_parser.*  Parser de `<model>`/`<silence>`, atributos, speakers y silencios.
markup/request_options.* Opciones float de requests JSON para markup/API.
markup/audio_parts.*    Piezas de audio, resampling, ensamblado WAV y JSON de segmentos.
wav_utils.*             Lectura/escritura WAV PCM y resampling lineal simple.
output_cleanup.*        Limpieza de temporales y retención de outputs.
```

## `src/cpp/text_normalizer.cpp` y `src/cpp/text/`

La API pública se mantiene en `text_normalizer.hpp`, pero la implementación queda dividida:

```text
text_normalizer.cpp          Orquestación pública: parse config + normalize.
text/string_utils.*          Utilidades ASCII, límites de palabra y puntuación final.
text/replacements.*          Reglas personalizadas por modelo y legacy replacements.
text/builtin_normalizer.*    Builtins protegidos: URL, email, versión, moneda, porcentaje y decimal.
```

La normalización sigue apagada para JSON clásicos. Solo se activa si el modelo declara `neo.text_normalization` o trae `modelcard.replacements` legacy.

## `src/cpp/neo_model.cpp` y `src/cpp/neo/`

La API pública de paquetes `.neo` se mantiene en `neo_model.hpp`. La implementación queda dividida así:

```text
neo_model.cpp            Fachada pública: inspect, extract, read image y write package.
neo/constants.hpp        Magic, versión y códigos de compresión.
neo/package_types.hpp    Tipos internos de sección y paquete.
neo/binary_io.*          Serialización little-endian y strings.
neo/file_utils.*         Lectura/escritura de archivos, lower-case y hash de caché.
neo/compression.*        zstd opcional y validación de tamaños.
neo/image_payload.*      MIME de imagen y data URI base64.
neo/package_reader.*     Parseo de directorio y lectura de secciones.
neo/package_writer.*     Exportación desde ONNX/config/imagen a `.neo`.
```

Esta separación evita que el formato `.neo` vuelva a mezclar parsing binario, compresión, extracción, imágenes y escritura en un solo archivo.

## Flujo de síntesis CLI/WAV

1. `app/cli_args.*` parsea flags, `app/cli_validation.*` valida rutas/configuración y `app/piper_app.*` selecciona modo.
2. `app/voice_runtime.*` prepara la voz, eSpeak/tashkeel y aplica overrides.
3. `app/synthesis_mode.*` resuelve entrada/salida: stdin, texto directo, archivo, JSON, WAV o RAW.
4. `loadVoice()` lee config JSON y carga el modelo mediante `core/model_runtime.*`.
5. `textToWavFile()` o `textToWavFileFromStream()` escriben header WAV progresivo.
6. `textToAudio()` normaliza texto por modelo si corresponde.
7. El pipeline aplica tashkeel opcional.
8. El pipeline fonemiza con eSpeak o codepoints.
9. Los fonemas se convierten a ids.
10. `model_runtime.*` ejecuta ONNX Runtime.
11. Se agregan silencios de frase/oración.
12. El writer WAV parchea el header si el stream es seekable.

## Flujo de una petición TTS HTTP

1. `server.cpp` acepta la conexión.
2. `request_handler.*` lee, autoriza y enruta la petición.
3. `auth.*` valida token si está configurado.
4. `markup/markup_parser.*` detecta y divide markup TTS en segmentos.
5. `markup_tts.*` coordina síntesis por segmento y mezcla final WAV.
6. `text_sanitizer.*` coordina `sanitize/*` para limpiar texto plano o segmentos y calcular riesgo.
7. `tts_scheduler.*` divide trabajo en chunks y administra cola/concurrencia.
8. `model_cache.*` entrega una voz disponible o carga réplica del modelo.
9. `piper/api.hpp` y `src/cpp/core/` ejecutan normalización, fonemización e inferencia.
10. `jobs/chunked_wav.*` ensambla los chunks RAW en un WAV final.
11. `metrics_report.*` expone métricas y política de recursos como JSON.
12. `request_handler.*` responde JSON con la URL del archivo generado.

## Límites conservados

- La API puede quedar abierta si no se define token, igual que antes.
- La salida de archivos sigue restringida por nombres seguros.
- `.neo` sigue exponiendo contrato público en `neo_model.*`, pero la implementación interna vive en `src/cpp/neo/`.
- La normalización de texto no muta configuración compartida durante síntesis.
- Los scripts de build locales siguen intactos.
- No se agregaron dependencias nuevas.

## Validaciones disponibles

```bash
python3 script/smoke-text-normalizer.py
python3 script/smoke-project-structure.py
cmake -S . -B /tmp/piper-neo-cmake-check -DPIPER_BUILD_TESTS=OFF
cmake -S . -B /tmp/piper-neo-cmake-check-tests -DPIPER_BUILD_TESTS=ON
cmake --build /tmp/piper-neo-cmake-check-tests --target test_neo_package
cmake --build /tmp/piper-neo-cmake-check-tests --target test_text_sanitizer
ctest --test-dir /tmp/piper-neo-cmake-check-tests -R "test_neo_package|test_text_sanitizer" --output-on-failure
```

## Pendientes recomendados

- Compilar con el script real de Windows.
- Probar síntesis real con `.onnx` y `.neo`.
- Probar `--output_raw`, WAV normal y stdin largo.
- Ampliar `script/smoke-piper-binary.py` con más casos cuando se agreguen endpoints o flags nuevos.
- Ampliar pruebas funcionales de `.neo` para cubrir export zstd real cuando zstd esté disponible.
- Agregar pruebas HTTP unitarias para `routes/*` usando un writer/socket simulado.
- `test_sentence_splitter` cubre abreviaturas, decimales, comillas, puntos suspensivos y saltos de línea.
- `test_model_registry` cubre escaneo/resolución de modelos con fakes `.onnx`, `.json` y `.neo`.
- Evaluar CMake moderno por targets si se decide tocar el sistema de build con más calma.

## Refactor HTTP y catálogo de modelos

El servidor HTTP base ya no concentra socket I/O, escritura de respuestas y helpers de URL en `server/http.cpp`. La capa quedó dividida en:

```text
src/cpp/server/http.cpp                  Lectura y parseo básico de request HTTP.
src/cpp/server/http/socket_io.cpp        E/S de sockets y cierre multiplataforma.
src/cpp/server/http/response_writer.cpp  Respuestas HTTP/JSON y envío de archivos.
src/cpp/server/http/url.cpp              Query params, URL decode y rutas dinámicas.
src/cpp/server/http_types.hpp            Tipos HTTP ligeros sin depender de ONNX/Piper.
```

El registro de modelos también se separó:

```text
src/cpp/server/model_registry.cpp   Cache, refresh y lookup por nombre.
src/cpp/server/model_metadata.cpp   Conversión de metadata/config a JSON público.
src/cpp/server/model_scanner.cpp    Escaneo de `.onnx` y `.neo` en `modelsDir`.
src/cpp/server/model_paths.cpp      Claves canónicas compartidas por registry/cache.
```

Esta separación mantiene el contrato público del servidor pero reduce acoplamiento y facilita pruebas unitarias pequeñas como `test_http_parser`.

## Seguridad de concurrencia eSpeak/tashkeel

El servidor puede procesar chunks y peticiones en paralelo, pero eSpeak-ng y piper-phonemize usan estado global del proceso. Para evitar errores intermitentes en Windows como `Bad data: es_dict length=0`, la fonemización eSpeak se serializa con un mutex global de alcance corto. El lock solo cubre la conversión texto -> fonemas; la inferencia ONNX puede seguir ejecutándose en paralelo por réplica de modelo.

`model_cache.*` también evita el stampede de carga inicial: si varios chunks piden el mismo modelo sin cache, solo un hilo carga la primera réplica y los demás esperan a que exista un slot usable antes de crear réplicas adicionales.

`script/smoke-piper-binary.py` incluye `--stress-api-requests` para lanzar peticiones TTS concurrentes con texto largo y detectar regresiones de eSpeak/model cache después de compilar el binario real.

## Refactor del pipeline de síntesis

`src/cpp/core/synthesis_pipeline.cpp` quedó como orquestador de alto nivel. Las etapas internas del pipeline viven en módulos dedicados:

```text
src/cpp/core/pipeline/text_processing.cpp     Normalización de texto y tashkeel protegido por mutex.
src/cpp/core/pipeline/phonemizer.cpp          Fonemización eSpeak/codepoints con protección de concurrencia.
src/cpp/core/pipeline/phrase_synthesizer.cpp  Split por silencios de fonemas, conversión a IDs e inferencia por frases.
```

La API pública `textToAudio()` no cambió. La serialización de eSpeak/tashkeel sigue siendo de alcance corto y la inferencia ONNX conserva paralelismo por réplica después de la fonemización.

## Refactor WAV y utilidades multimedia

El streaming WAV del core quedó separado para evitar que `wav_stream_writer.cpp` concentre header, escritura incremental, lectura de stream y síntesis por chunks:

```text
src/cpp/core/wav_stream_writer.cpp        Orquestador público de WAV.
src/cpp/core/wav/wav_header_writer.*      Header RIFF/WAVE temporal y parcheo final.
src/cpp/core/wav/stream_chunks.*          Síntesis de chunks hacia std::ostream.
```

Las utilidades multimedia del servidor también fueron separadas:

```text
src/cpp/server/media/base64.*      Decodificación Base64.
src/cpp/server/media/data_image.*  Parseo de data:image/...;base64 usado por model cards.
```

`server/utils.*` queda limitado a utilidades generales: tiempo, strings, nombres seguros, JSON y nombres de salida.

## Refactor de request TTS

La ruta `/api/v1/tts` mantiene `handleTtsRoute()` como entrada pública, pero el parseo de request y el payload de respuesta quedaron separados:

```text
src/cpp/server/routes/tts_request.*  JSON, validación de campos, opciones y límites.
src/cpp/server/routes/tts_payload.*  Payload de éxito compartido por TTS plain y markup.
src/cpp/server/routes/tts_routes.*   Dispatch, sanitizer, scheduler, markup y limpieza de errores.
```

Esto reduce el acoplamiento de la ruta y permite agregar pruebas unitarias de request sin levantar sockets.

## CLI synthesis mode helpers

The CLI synthesis mode is intentionally kept as a small orchestration layer. The
helper modules under `src/cpp/app/` split input handling, JSON-line overrides,
output writing and timestamped paths:

- `synthesis_input.*`: direct input from `--text`, `--input_file` or stdin.
- `synthesis_json.*`: per-line JSON overrides such as `output_file`, `speaker_id`
  and named `speaker`.
- `synthesis_output.*`: WAV/stdout/raw output handling and synthesis result logs.
- `synthesis_paths.*`: timestamped output paths for directory mode.

## Builtin text normalization modules

Builtin text normalization is split to keep model-controlled speech rules easy to
test:

- `text/builtin_matchers.*`: regex patterns and safe token boundaries.
- `text/builtin_renderers.*`: spoken forms for URLs, emails, versions, currency
  and percentages.
- `text/protected_segments.*`: temporary markers that protect builtin output
  before custom replacements are applied.
- `tests/test_text_builtins.cpp`: fast unit coverage without ONNX, eSpeak or
  piper-phonemize.


## Cierre de refactor

- `src/cpp/app/platform_console.cpp` concentra la consola UTF-8 en Windows.
- `src/cpp/app/platform_paths.cpp` concentra la resolución de ruta del ejecutable.
- `script/smoke-piper-binary.py` valida CLI avanzado, API HTTP y stress concurrente con modelos reales.
- El core no convierte números, moneda, porcentajes ni versiones; esa responsabilidad queda en `replacements` por modelo.
