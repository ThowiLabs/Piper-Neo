# Fecha

18 de junio de 2026

# Objetivo

Separar la capa de argumentos CLI para que `cli_args.cpp` deje de mezclar texto de ayuda, parsing de flags, validación cruzada, resolución de rutas y configuración derivada.

# Decisiones tomadas

- `cli_args.cpp` queda enfocado en convertir argumentos de entrada a `RunConfig`.
- `help_text.*` concentra la salida de `--help`.
- `cli_validation.*` concentra validaciones posteriores al parseo: entrada incompatible, archivo de texto, modo servidor, búsqueda de modelo, detección `.neo`, config `.onnx.json` y exportación `.neo`.
- Se conserva el contrato público `parseArgs(argc, argv, runConfig)`.
- No se cambian flags existentes ni se modifican los scripts de build.

# Arquitectura actual

```text
src/cpp/app/cli_args.*          Parser CLI y asignación directa a RunConfig.
src/cpp/app/help_text.*         Texto de ayuda visible para usuario.
src/cpp/app/cli_validation.*    Validación cruzada y resolución final de modelo/config.
```

# Librerías usadas

No se agregaron dependencias. Se mantiene C++17 y las dependencias ya existentes del proyecto.

# Archivos importantes modificados

- `src/cpp/app/cli_args.cpp`
- `src/cpp/app/cli_args.hpp`
- `src/cpp/app/help_text.hpp`
- `src/cpp/app/help_text.cpp`
- `src/cpp/app/cli_validation.hpp`
- `src/cpp/app/cli_validation.cpp`
- `CMakeLists.txt`
- `script/smoke-project-structure.py`
- `docs/core-architecture.md`
- `docs/auditoria-refactor-piper-neo.md`

# Problemas encontrados

`cli_args.cpp` tenía más de 400 líneas y concentraba responsabilidades distintas. Eso hacía más riesgoso agregar o corregir flags porque cualquier cambio podía afectar help, validación, servidor, export `.neo` o carga de modelo.

# Soluciones implementadas

Se separó la ayuda y la validación posterior al parseo en módulos propios, manteniendo compatibilidad del punto de entrada `parseArgs`. El smoke test estructural ahora verifica que los nuevos módulos existan y estén registrados en CMake.

# Pendientes

- Agregar pruebas funcionales de CLI para `--help`, `--version`, `--text`, `--input_file`, `--server`, `--export-neo` y combinaciones inválidas.
- Revisar si conviene rechazar flags desconocidos en una versión futura; no se cambió ese comportamiento para evitar romper compatibilidad.

# Próximos pasos

Continuar con pruebas funcionales `.neo` y CLI antes de seguir con refactors de concurrencia en `server/tts_scheduler.cpp` o con la limpieza pesada de `piper.hpp`.
