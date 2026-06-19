# Formato `.neo`

`.neo` es el paquete propio de Piper Neo para distribuir un modelo con sus metadatos.

Un paquete puede incluir:

- modelo ONNX
- configuración JSON del modelo
- imagen/model card opcional
- metadatos internos
- secciones comprimidas con zstd cuando aplique

## Implementación

La fachada pública vive en:

```text
src/cpp/neo_model.hpp
src/cpp/neo_model.cpp
```

La implementación interna está separada en:

```text
src/cpp/neo/constants.hpp
src/cpp/neo/package_types.hpp
src/cpp/neo/binary_io.*
src/cpp/neo/file_utils.*
src/cpp/neo/compression.*
src/cpp/neo/image_payload.*
src/cpp/neo/package_reader.*
src/cpp/neo/package_writer.*
```

## Operaciones

El core soporta:

- inspeccionar un `.neo`
- extraer ONNX/config a temporales seguros
- leer imagen embebida
- escribir `.neo` desde ONNX/config/imagen

## Prueba mínima

El test funcional está en:

```text
src/cpp/tests/test_neo_package.cpp
```

Valida:

- creación de paquete mínimo
- inspección
- extracción ONNX/config
- lectura de imagen embebida

## Regla de compatibilidad

`neo_model.cpp` debe seguir siendo fachada estable. Nuevas reglas internas deben vivir en `src/cpp/neo/`.
