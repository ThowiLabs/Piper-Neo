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
hardware.*              Detección de CPU, memoria y límites cgroup.
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
text_chunker.cpp        Particionado inteligente UTF-8 para textos largos.
sentence_splitter.hpp   Contrato interno para pausas explícitas.
sentence_splitter.cpp   Detección de oración, abreviaturas, decimales y versiones.
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
text/spanish_numbers.*       Conversión numérica básica a español.
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
