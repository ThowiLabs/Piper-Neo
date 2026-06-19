# Fecha
2026-06-18

# Objetivo
Reducir responsabilidades mezcladas en el registro/catálogo de modelos del servidor y compartir utilidades de rutas de modelo entre registry y cache.

# Decisiones tomadas
- Mantener `ModelRegistry` como cache temporal y punto de lookup.
- Mover la conversión `ModelInfo -> JSON` a `model_metadata.*`.
- Mover el escaneo de carpetas y selección del primer modelo usable a `model_scanner.*`.
- Mover la generación de clave canónica de modelo a `model_paths.*` para evitar duplicación con `model_cache.cpp`.
- Mantener `model_registry.hpp` compatible reexportando los helpers usados por las rutas.

# Arquitectura actual
```text
server/model_registry.*  Cache temporal, refresh y búsqueda por nombre.
server/model_metadata.*  Metadata JSON, modelcard, imagen y campos técnicos.
server/model_scanner.*   Escaneo de modelsDir para .onnx/.neo y primer modelo usable.
server/model_paths.*     Clave canónica de modelo para cache de runtime.
server/model_cache.*     Carga y préstamo de voces/modelos usando modelKey compartido.
```

# Librerías usadas
- C++17 standard library.
- `json.hpp` vendorizado.
- Módulo `.neo` existente para inspección de paquetes.

# Archivos importantes modificados
- `src/cpp/server/model_registry.cpp`
- `src/cpp/server/model_registry.hpp`
- `src/cpp/server/model_metadata.hpp`
- `src/cpp/server/model_metadata.cpp`
- `src/cpp/server/model_scanner.hpp`
- `src/cpp/server/model_scanner.cpp`
- `src/cpp/server/model_paths.hpp`
- `src/cpp/server/model_paths.cpp`
- `src/cpp/server/model_cache.cpp`
- `CMakeLists.txt`
- `script/smoke-project-structure.py`

# Problemas encontrados
`model_registry.cpp` hacía demasiadas cosas: cache, JSON público, lectura de config, inspección `.neo`, escaneo de carpeta, búsqueda por nombre y clave canónica. Además `model_cache.cpp` tenía su propia copia de `modelKey`.

# Soluciones implementadas
- Separación por módulos pequeños.
- Eliminación de la copia local de `modelKey` en `model_cache.cpp`.
- Smoke estructural actualizado para exigir los nuevos módulos.

# Pendientes
- Agregar tests funcionales para `scanModels`, `modelInfoToJson` y resolución de modelos.
- Revisar `model_cache.cpp` con pruebas de concurrencia antes de separar carga/slots/leases.

# Próximos pasos
Continuar con `tts_scheduler.cpp` o agregar pruebas de registry/cache antes de más cambios de concurrencia.
