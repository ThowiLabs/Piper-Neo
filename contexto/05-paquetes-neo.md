# Fecha

18 de junio de 2026

# Objetivo

Documentar el soporte de paquetes de voz `.neo` después de separar su implementación interna en módulos pequeños.

# Decisiones tomadas

- `.neo` convive con voces clásicas `.onnx` + `.onnx.json`.
- El contrato público sigue en `src/cpp/neo_model.hpp` para no romper CLI, servidor ni consumidores internos.
- La implementación dejó de concentrarse en `neo_model.cpp` y ahora vive en `src/cpp/neo/`.
- zstd sigue siendo opcional en build, pero necesario para compresión/descompresión real de secciones `.neo`.
- No se agregó ninguna dependencia nueva.

# Arquitectura actual

```text
src/cpp/neo_model.hpp          contrato público de paquetes .neo
src/cpp/neo_model.cpp          fachada pública estable
src/cpp/neo/constants.hpp      magic, versión y códigos de compresión
src/cpp/neo/package_types.hpp  tipos internos de sección/paquete
src/cpp/neo/binary_io.*        lectura/escritura little-endian y strings
src/cpp/neo/file_utils.*       lectura/escritura de archivos, hash y utilidades de path
src/cpp/neo/compression.*      zstd opcional y descompresión por sección
src/cpp/neo/image_payload.*    MIME de imagen y data URI base64
src/cpp/neo/package_reader.*   parsing, búsqueda y lectura de secciones
src/cpp/neo/package_writer.*   exportación de ONNX/config/imagen a .neo
neo-docs/neo-format.md         documentación del formato
```

# Librerías usadas

- C++17.
- zstd opcional.
- nlohmann/json vendorizado.

# Archivos importantes modificados

- `src/cpp/neo_model.cpp`
- `src/cpp/neo_model.hpp`
- `src/cpp/neo/*`
- `CMakeLists.txt`
- `script/smoke-project-structure.py`
- `neo-docs/README.md`
- `README.md`
- `README.es.md`
- `docs/core-architecture.md`

# Problemas encontrados

- `neo_model.cpp` concentraba lectura binaria, escritura binaria, validación, compresión, extracción, imágenes y exportación.
- Esa concentración hacía más riesgoso corregir errores del formato o agregar pruebas.

# Soluciones implementadas

- Se separó `.neo` por responsabilidades reales.
- `neo_model.cpp` quedó como fachada pública de compatibilidad.
- CMake ahora tiene `PIPER_NEO_SOURCES`.
- El smoke estructural valida que los módulos `.neo` existan y estén referenciados en CMake.

# Pendientes

- Agregar pruebas funcionales de `.neo` con zstd disponible.
- Probar exportación real desde `.onnx` + `.onnx.json`.
- Probar inspección, extracción, imagen embebida y caché.
- Validar build con y sin zstd en Windows.

# Próximos pasos

Continuar con `piper.hpp`, `app/cli_args.cpp` o pruebas funcionales de `.neo` según prioridad.
