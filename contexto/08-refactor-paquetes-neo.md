# Fecha

18 de junio de 2026

# Objetivo

Registrar el refactor aplicado al subsistema de paquetes `.neo` para que el repo público tenga una arquitectura más mantenible.

# Decisiones tomadas

- Mantener `src/cpp/neo_model.hpp` como API pública estable.
- Convertir `src/cpp/neo_model.cpp` en fachada del subsistema.
- Crear `src/cpp/neo/` para módulos internos del formato.
- Evitar interfaces/factories innecesarias: son funciones directas y archivos pequeños.
- No modificar scripts de build locales.

# Arquitectura actual

```text
src/cpp/neo_model.cpp          fachada pública
src/cpp/neo/binary_io.*        primitivas de serialización
src/cpp/neo/file_utils.*       archivos y utilidades simples
src/cpp/neo/compression.*      zstd opcional
src/cpp/neo/image_payload.*    imagen/data URI
src/cpp/neo/package_reader.*   lectura e inspección de secciones
src/cpp/neo/package_writer.*   escritura del paquete
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
- `docs/core-architecture.md`
- `docs/auditoria-refactor-piper-neo.md`
- `README.md`
- `README.es.md`

# Problemas encontrados

- El archivo `.neo` original era autocontenido pero demasiado grande.
- Los errores de lectura/escritura/compression podían mezclarse en un solo punto.

# Soluciones implementadas

- Separación en módulos internos sin cambiar el contrato público.
- Validación sintáctica local de los módulos `.neo` con C++17.
- Actualización de documentación para reflejar la arquitectura real.

# Pendientes

- Probar empaquetado `.neo` real en Windows con zstd.
- Agregar prueba automatizada que cree un `.neo` mínimo, lo inspeccione y lo extraiga.
- Revisar si conviene agregar límites explícitos de tamaño por sección.

# Próximos pasos

Seguir con limpieza del header público `piper.hpp` o con pruebas funcionales `.neo` antes de tocar más runtime.
