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
src/cpp/piper.cpp
src/cpp/core/
src/cpp/text_normalizer.cpp
src/cpp/text/
src/cpp/neo_model.cpp
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
cli_args.*              Parsing y validación de argumentos.
hardware.*              Detección de CPU, memoria y límites cgroup.
env.*                   Resolución de token desde argumento, entorno o .env.
platform.*              Detalles del ejecutable y consola por plataforma.
raw_audio_output.*      Escritura progresiva de audio RAW a stdout.
piper_app.*             Orquestación de CLI, server mode y exportación .neo.
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

`src/cpp/piper.cpp` queda mínimo y conserva solo `getVersion()`. El contrato público sigue en `piper.hpp`, así que CLI, servidor y tests no cambian su forma de integración.

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
text_sanitizer.*        Limpieza segura de texto antes de síntesis API.
model_registry.*        Escaneo, listado y metadatos de modelos.
model_cache.*           Réplicas de modelos y leases de voces.
tts_scheduler.*         Cola justa, concurrencia, métricas y ensamblado de WAV.
markup_tts.*            Parser de markup TTS, silencios y mezcla de segmentos.
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

## Flujo de síntesis CLI/WAV

1. `app/piper_app.*` resuelve argumentos, entrada y salida.
2. `loadVoice()` lee config JSON y carga el modelo mediante `core/model_runtime.*`.
3. `textToWavFile()` o `textToWavFileFromStream()` escriben header WAV progresivo.
4. `textToAudio()` normaliza texto por modelo si corresponde.
5. El pipeline aplica tashkeel opcional.
6. El pipeline fonemiza con eSpeak o codepoints.
7. Los fonemas se convierten a ids.
8. `model_runtime.*` ejecuta ONNX Runtime.
9. Se agregan silencios de frase/oración.
10. El writer WAV parchea el header si el stream es seekable.

## Flujo de una petición TTS HTTP

1. `server.cpp` acepta la conexión.
2. `request_handler.*` lee, autoriza y enruta la petición.
3. `auth.*` valida token si está configurado.
4. `markup_tts.*` detecta si el texto usa markup TTS.
5. `text_sanitizer.*` limpia texto plano o segmentos.
6. `tts_scheduler.*` divide trabajo en chunks y administra cola/concurrencia.
7. `model_cache.*` entrega una voz disponible o carga réplica del modelo.
8. `piper.hpp`/`src/cpp/core/` ejecutan normalización, fonemización e inferencia.
9. `wav_utils.*` ensambla audio WAV de la API.
10. `request_handler.*` responde JSON con la URL del archivo generado.

## Límites conservados

- La API puede quedar abierta si no se define token, igual que antes.
- La salida de archivos sigue restringida por nombres seguros.
- `.neo` sigue gestionado por `neo_model.*`.
- La normalización de texto no muta configuración compartida durante síntesis.
- Los scripts de build locales siguen intactos.
- No se agregaron dependencias nuevas.

## Validaciones disponibles

```bash
python3 script/smoke-text-normalizer.py
python3 script/smoke-project-structure.py
cmake -S . -B /tmp/piper-neo-cmake-check -DPIPER_BUILD_TESTS=OFF
cmake -S . -B /tmp/piper-neo-cmake-check-tests -DPIPER_BUILD_TESTS=ON
```

## Pendientes recomendados

- Compilar con el script real de Windows.
- Probar síntesis real con `.onnx` y `.neo`.
- Probar `--output_raw`, WAV normal y stdin largo.
- Agregar smoke tests HTTP para `/api/health`, `/api/v1/models`, `/api/v1/tts` y archivos.
- Separar `request_handler.*` por endpoint solo si crecen rutas o pruebas HTTP.
- Evaluar CMake moderno por targets si se decide tocar el sistema de build con más calma.
