# Fecha
2026-06-19

# Objetivo
Reducir responsabilidades mezcladas en `model_cache.cpp` y separar runtime/leases de modelos de la carga real de voces.

# Decisiones tomadas
- Mantener `ModelCache` como orquestador de checkout y cache por clave canónica.
- Mover `VoiceSlot`, `ModelRuntime` y `VoiceLease` a `model_runtime.*`.
- Mover carga real de modelos `.onnx`/`.neo` a `model_loader.*`.
- Mantener `model_cache.hpp` compatible exponiendo `VoiceLease` mediante `model_runtime.hpp`.

# Arquitectura actual
```text
server/model_cache.*     Orquesta cache, checkout y resolución de modelos.
server/model_runtime.*   Slots, runtime compartido y lease RAII de voz.
server/model_loader.*    Carga piper::Voice desde .onnx/.json o .neo extraído.
server/model_paths.*     Clave canónica de modelo.
server/model_registry.*  Lookup de modelos disponibles.
```

# Librerías usadas
- C++17 standard library.
- `spdlog` para logs de carga de modelos.
- API pública de Piper para `loadVoice`.
- Módulo `.neo` para extracción temporal de paquetes.

# Archivos importantes modificados
- `src/cpp/server/model_cache.hpp`
- `src/cpp/server/model_cache.cpp`
- `src/cpp/server/model_runtime.hpp`
- `src/cpp/server/model_runtime.cpp`
- `src/cpp/server/model_loader.hpp`
- `src/cpp/server/model_loader.cpp`
- `CMakeLists.txt`
- `script/smoke-project-structure.py`

# Problemas encontrados
`model_cache.cpp` todavía mezclaba lifecycle de leases, runtime de slots, carga de modelos, extracción `.neo`, logging y coordinación de réplicas.

# Soluciones implementadas
- `model_cache.cpp` quedó más pequeño y enfocado en cache/checkout.
- `VoiceLease` conserva RAII para liberar slots al destruirse.
- La carga de modelos quedó aislada para facilitar pruebas y futuros cambios.

# Pendientes
- Crear pruebas con fakes/mocks para cache y lease sin requerir ONNX real.
- Validar concurrencia de múltiples peticiones al mismo modelo con el binario final.

# Próximos pasos
Seguir con pruebas funcionales del servidor real y, después, evaluar `text_chunker.cpp` con casos de texto largo.
