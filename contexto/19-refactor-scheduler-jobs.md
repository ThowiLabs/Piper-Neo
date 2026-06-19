# Fecha
2026-06-19

# Objetivo
Reducir la responsabilidad de `tts_scheduler.cpp` y separar la lógica de jobs/chunks sin cambiar el contrato público de `FairTtsScheduler` ni los endpoints HTTP.

# Decisiones tomadas
- Mantener `FairTtsScheduler` como coordinador de cola, activación, espera y workers.
- Extraer el estado compartido de job a `server/jobs/job_state.hpp`.
- Extraer creación, cierre, limpieza temporal, resultado y ensamblado WAV a `server/jobs/job_lifecycle.*`.
- Extraer la síntesis real de chunks a `server/jobs/chunk_worker.*`.
- Mantener el algoritmo de round-robin justo y las métricas existentes.
- Usar RAII para restaurar overrides temporales de voz después de cada chunk.

# Arquitectura actual
```text
server/tts_scheduler.*       Coordina cola justa, workers, activación y espera de jobs.
server/jobs/job_state.*      Estado compartido de jobs y work items.
server/jobs/job_lifecycle.*  Crea jobs, marca chunks, ensambla WAV, limpia temp y arma resultados.
server/jobs/chunk_worker.*   Ejecuta síntesis por chunk, aplica overrides temporales y actualiza métricas.
server/jobs/chunked_wav.*    Une chunks RAW en WAV final.
```

# Librerías usadas
- C++17 standard library.
- `spdlog` para logs existentes.
- API pública de Piper (`piper.hpp`).

# Archivos importantes modificados
- `src/cpp/server/tts_scheduler.cpp`
- `src/cpp/server/tts_scheduler.hpp`
- `src/cpp/server/jobs/job_state.hpp`
- `src/cpp/server/jobs/job_lifecycle.hpp`
- `src/cpp/server/jobs/job_lifecycle.cpp`
- `src/cpp/server/jobs/chunk_worker.hpp`
- `src/cpp/server/jobs/chunk_worker.cpp`
- `CMakeLists.txt`
- `script/smoke-project-structure.py`

# Problemas encontrados
`tts_scheduler.cpp` todavía mezclaba estado, creación de jobs, ejecución de chunks, restauración de configuración de voz, ensamblado WAV y limpieza temporal. Esa mezcla hacía más riesgoso tocar concurrencia o añadir pruebas.

# Soluciones implementadas
- Se redujo `tts_scheduler.cpp` a coordinación de cola y workers.
- Se movió la síntesis por chunk a un módulo específico.
- Se mantuvo el comportamiento de cancelación, errores y métricas.
- Se agregó validación estructural para evitar que `tts_scheduler.cpp` vuelva a crecer demasiado.

# Pendientes
- Agregar pruebas de concurrencia del scheduler con mocks/fakes de modelos.
- Probar con modelos reales `.onnx` y `.neo` en Windows usando el smoke del binario final.

# Próximos pasos
Usar `script/smoke-piper-binary.py --models <ruta>` después del build real para validar CLI, API y generación WAV con modelos reales.
