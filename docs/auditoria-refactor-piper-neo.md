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

## Deuda técnica restante

### Alta prioridad

1. `src/cpp/neo_model.cpp`: ya quedó como fachada; seguir con pruebas funcionales de `.neo`.
2. `src/cpp/piper.hpp`: reducir includes pesados con una fachada compatible.
3. `src/cpp/app/synthesis_mode.cpp`: agregar pruebas de stdin, JSON input, output WAV y RAW.
4. Pruebas CLI: validar combinaciones de flags, `--help`, `--version`, `--server` y export `.neo`.

### Prioridad media

1. `src/cpp/server/tts_scheduler.cpp`: separar cola, estado, workers, métricas y ensamblado.
2. `src/cpp/server/markup_tts.cpp`: separar parser, validación, render y mezcla WAV.
3. `src/cpp/server/text_sanitizer.cpp`: separar detectores y risk score.
4. `src/cpp/server/request_handler.cpp`: separar rutas si el API crece.

### Prioridad baja

1. `src/cpp/core/text_chunker.cpp`: separar helpers UTF-8 y selección de cortes.
2. `src/cpp/core/synthesis_pipeline.cpp`: separar stages cuando existan pruebas de audio suficientes.
3. `src/cpp/app/hardware.cpp`: separar probe, perfiles y límites.

## Pruebas recomendadas antes de más refactor

- `.neo`: exportar, leer, extraer metadata, extraer imagen y validar zstd/no-zstd.
- Chunking: textos largos, signos `¿?`, `¡!`, URLs, decimales, abreviaturas y saltos de línea.
- HTTP: GET, POST, query params, payload grande, headers malformados.
- Sanitizer: texto normal, código, URLs largas, emoji, input vacío e input enorme.
- Audio real: `.onnx`, `.neo`, `--output_raw`, stdin largo y servidor local.

## Estado recomendado

El repo está listo como base pública del motor Piper Neo. El refactor grande de `.neo`, la separación de modos de aplicación y la limpieza de CLI ya fueron aplicados; el siguiente paso recomendable es agregar pruebas funcionales del formato `.neo` y pruebas pequeñas de CLI antes de tocar más runtime.
