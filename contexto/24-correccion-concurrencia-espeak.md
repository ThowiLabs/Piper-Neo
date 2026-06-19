# Fecha

19 de junio de 2026

# Objetivo

Corregir una regresión de concurrencia en el servidor TTS detectada con textos largos y varios modelos eSpeak.

# Decisiones tomadas

- Serializar la llamada a `phonemize_eSpeak()` porque eSpeak-ng/piper-phonemize usa estado global del proceso.
- Serializar la llamada a libtashkeel porque comparte un `State` mutable en `PiperConfig`.
- Mantener el lock con alcance corto: solo fonemización/diacritización, no la inferencia ONNX completa.
- Evitar el stampede de carga inicial del mismo modelo en `ModelCache`: si ya hay una réplica cargándose, los demás hilos esperan antes de iniciar otra carga.
- Ampliar `script/smoke-piper-binary.py` con `--stress-api-requests` para reproducir peticiones TTS concurrentes después del build real.

# Arquitectura actual

- `src/cpp/core/synthesis_pipeline.cpp` contiene locks internos para proteger recursos globales de eSpeak/tashkeel.
- `src/cpp/server/model_cache.cpp` permite varias réplicas, pero no arranca varias cargas iniciales simultáneas para el mismo modelo.
- El scheduler puede seguir usando varios workers; la fonemización se serializa y la inferencia ONNX puede continuar en paralelo por réplica.

# Librerías usadas

- C++17 standard library: `std::mutex`, `std::lock_guard`, `std::condition_variable`.
- eSpeak-ng/piper-phonemize existentes.
- libtashkeel existente.

# Archivos importantes modificados

- `src/cpp/core/synthesis_pipeline.cpp`
- `src/cpp/server/model_cache.cpp`
- `script/smoke-piper-binary.py`
- `docs/core-architecture.md`
- `docs/auditoria-refactor-piper-neo.md`
- `README.md`
- `README.es.md`
- `contexto/README.md`

# Problemas encontrados

En modo API, un texto largo dividido en varios chunks podía provocar varias cargas simultáneas del mismo modelo y varias fonemizaciones concurrentes. En Windows se observó:

```text
Bad data: '...\espeak-ng-data\es_dict' (... length=0)
```

También se observaban múltiples logs `Model load started` para el mismo modelo en el mismo instante.

# Soluciones implementadas

- Mutex global interno para `phonemize_eSpeak()`.
- Mutex global interno para `tashkeel_run()`.
- `ModelCache::checkout()` espera cuando ya hay una carga en curso para el mismo `ModelRuntime`.
- El smoke del binario final ejecuta stress concurrente por defecto con dos peticiones API largas.

# Pendientes

- Validar el build real de Windows con modelos `.onnx` y `.json`.
- Ejecutar `script/smoke-piper-binary.py --models <ruta> --stress-api-requests 4` contra el binario final.
- Si se necesita más paralelismo en el futuro, investigar si piper-phonemize permite contextos eSpeak independientes por hilo.

# Próximos pasos

Después de validar esta corrección, continuar solo con refactors que no afecten fonemización/inferencia o crear pruebas adicionales antes de tocar `synthesis_pipeline.cpp`.
