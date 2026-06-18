# Fecha

18 de junio de 2026

# Objetivo

Continuar la refactorización del motor Piper Neo reforzando tres áreas: contrato público del core, separación menor del scheduler HTTP y pruebas funcionales mínimas del formato `.neo`.

# Decisiones tomadas

- `src/cpp/piper.hpp` queda como fachada pública compatible.
- Los tipos públicos se separan en `src/cpp/piper/types.hpp`.
- Las funciones públicas se separan en `src/cpp/piper/api.hpp`.
- Los módulos internos del core deben incluir `piper/types.hpp` o `piper/api.hpp` según lo que necesiten.
- Los reportes JSON de métricas del servidor ya no viven dentro de `tts_scheduler.cpp`.
- El ensamblado de WAV desde chunks del scheduler se mueve a un módulo específico bajo `server/jobs/`.
- Se agrega una prueba C++ funcional para leer, inspeccionar, extraer e imagen de un paquete `.neo` sin depender de ONNX real.

# Arquitectura actual

- `piper.hpp`: fachada de compatibilidad.
- `piper/types.hpp`: estructuras públicas como `PiperConfig`, `Voice`, `SynthesisConfig`, `ModelSession` y `SynthesisResult`.
- `piper/api.hpp`: funciones públicas como `initialize`, `loadVoice`, `textToAudio`, `textToWavFile` y `splitTextIntoChunks`.
- `server/metrics_report.*`: JSON de política de recursos y métricas.
- `server/jobs/chunked_wav.*`: ensamblado de WAV desde chunks RAW generados por el scheduler.
- `tests/test_neo_package.cpp`: prueba funcional de paquete `.neo` sin modelo real.

# Librerías usadas

No se agregaron dependencias nuevas. Se mantiene C++17 y las dependencias existentes del proyecto.

# Archivos importantes modificados

- `src/cpp/piper.hpp`
- `src/cpp/piper/types.hpp`
- `src/cpp/piper/api.hpp`
- `src/cpp/server/tts_scheduler.cpp`
- `src/cpp/server/tts_scheduler.hpp`
- `src/cpp/server/metrics_report.hpp`
- `src/cpp/server/metrics_report.cpp`
- `src/cpp/server/jobs/chunked_wav.hpp`
- `src/cpp/server/jobs/chunked_wav.cpp`
- `src/cpp/server/request_handler.cpp`
- `src/cpp/tests/test_neo_package.cpp`
- `CMakeLists.txt`
- `script/smoke-project-structure.py`

# Problemas encontrados

- `piper.hpp` seguía mezclando tipos, API pública y dependencias pesadas en un único header.
- `tts_scheduler.cpp` mantenía lógica de métricas JSON y ensamblado de WAV además de cola/workers.
- El formato `.neo` ya estaba modular, pero faltaba una prueba funcional que no dependiera de modelos ONNX reales.

# Soluciones implementadas

- Se creó una fachada pública limpia para Piper: `piper.hpp` incluye `piper/api.hpp`.
- Se separaron tipos y API pública sin romper compatibilidad de includes existentes.
- Se movió `resourcePolicyJson()` y `metricsJson()` a `server/metrics_report.*`.
- Se movió `assembleChunkedWav()` a `server/jobs/chunked_wav.*`.
- Se agregó `test_neo_package`, que crea un `.neo` mínimo sin compresión, lo inspecciona, lee imagen y extrae modelo/config.
- Se reforzó `smoke-project-structure.py` para evitar regresiones de estos refactors.

# Pendientes

- Probar `test_neo_package` dentro del workflow si se decide activar tests C++ en CI.
- Seguir separando `tts_scheduler.cpp` si crece la lógica de jobs/workers.
- Agregar pruebas de CLI real para `--help`, `--version`, `--export-neo`, `--server` y combinaciones inválidas.
- Agregar pruebas HTTP pequeñas para `/api/health`, `/api/v1/models`, `/api/v1/tts` y descargas.

# Próximos pasos

El siguiente bloque recomendable es `server/text_sanitizer.cpp` o `server/markup_tts.cpp`, pero conviene hacerlo acompañado de pruebas específicas para no cambiar cómo se limpian URLs, correos, código, emojis o segmentos markup.
