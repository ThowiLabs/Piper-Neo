
# Fecha

18 de junio de 2026

# Objetivo

Documentar el soporte de paquetes de voz `.neo`.

# Decisiones tomadas

- `.neo` convive con voces clásicas `.onnx` + `.onnx.json`.
- El formato agrupa modelo, config, model card, imagen opcional y metadata.
- zstd es opcional en build, pero necesario para compresión/descompresión real de secciones.

# Arquitectura actual

```text
src/cpp/neo_model.hpp   contrato público de paquetes .neo
src/cpp/neo_model.cpp   lectura, inspección, extracción y escritura
neo-docs/neo-format.md  documentación del formato
```

# Librerías usadas

- C++17.
- zstd opcional.
- nlohmann/json vendorizado.

# Archivos importantes modificados

- `src/cpp/neo_model.cpp`
- `src/cpp/neo_model.hpp`
- `neo-docs/neo-format.md`
- `README.md`
- `README.es.md`

# Problemas encontrados

- `neo_model.cpp` todavía concentra lectura, escritura, validación, compresión, extracción e imágenes.
- Al ser una feature propia del proyecto, necesita pruebas antes de refactor más fino.

# Soluciones implementadas

- Se mantuvo el contrato estable para no romper CLI/API.
- Se documentó el flujo en README y `neo-docs/`.

# Pendientes

- Separar `neo_model.cpp` en reader/writer/compression/image payload.
- Agregar pruebas de exportación, lectura, metadata e imagen.
- Verificar build con y sin zstd.

# Próximos pasos

Refactorizar `.neo` después de crear pruebas mínimas del formato.
