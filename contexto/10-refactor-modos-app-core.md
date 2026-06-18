# Fecha

18 de junio de 2026

# Objetivo

Continuar la refactorización del motor Piper Neo separando la capa de aplicación CLI en modos de ejecución claros sin tocar `apps/`, workflows ni scripts locales de build.

# Decisiones tomadas

- `src/cpp/app/piper_app.cpp` queda como orquestador mínimo.
- La exportación `.neo` se mueve a `export_neo_mode.*`.
- La preparación de runtime, carga de voz, eSpeak, tashkeel y overrides de síntesis se mueve a `voice_runtime.*`.
- El modo servidor se mueve a `server_mode.*`.
- La síntesis CLI/stdin/JSON/RAW/WAV se mueve a `synthesis_mode.*`.
- Se agrega un guard interno para llamar `piper::terminate()` al salir del modo de ejecución normal.
- No se agregan dependencias nuevas.
- No se modifica `script/build-windows.py`.

# Arquitectura actual

La capa `src/cpp/app/` queda organizada así:

```text
piper_app.*             Entrada de aplicación y selección de modo.
cli_args.*              Parser de argumentos y asignación directa a RunConfig.
help_text.*             Texto de ayuda visible para usuario.
cli_validation.*        Validación cruzada, rutas y resolución final de modelo/config.
run_config.hpp          Configuración común de ejecución.
export_neo_mode.*       Exportación de ONNX/config/imagen a paquete `.neo`.
voice_runtime.*         Carga de modelo/voz y configuración de eSpeak/tashkeel.
server_mode.*           Mapeo de RunConfig hacia ServerOptions y ejecución HTTP.
synthesis_mode.*        Síntesis CLI por stdin, texto directo, archivo, JSON y RAW.
hardware.*              Detección de recursos y perfiles automáticos.
env.*                   Resolución de token API.
platform.*              Consola UTF-8 y path del ejecutable.
raw_audio_output.*      Thread de salida RAW progresiva.
```

# Librerías usadas

Se mantienen C++17, spdlog, nlohmann/json vendorizado y las dependencias existentes del motor.

# Archivos importantes modificados

- `CMakeLists.txt`
- `src/cpp/app/piper_app.cpp`
- `src/cpp/app/export_neo_mode.hpp`
- `src/cpp/app/export_neo_mode.cpp`
- `src/cpp/app/voice_runtime.hpp`
- `src/cpp/app/voice_runtime.cpp`
- `src/cpp/app/server_mode.hpp`
- `src/cpp/app/server_mode.cpp`
- `src/cpp/app/synthesis_mode.hpp`
- `src/cpp/app/synthesis_mode.cpp`
- `script/smoke-project-structure.py`
- `docs/core-architecture.md`
- `docs/auditoria-refactor-piper-neo.md`

# Problemas encontrados

`piper_app.cpp` todavía mezclaba carga de voz, exportación `.neo`, configuración de eSpeak/tashkeel, modo servidor, modo archivo/stdin, JSON input y RAW output. Esto hacía difícil modificar un modo sin riesgo de afectar otro.

# Soluciones implementadas

Se separaron los modos por responsabilidad real y `piper_app.cpp` quedó como selector de modo:

1. Parsear argumentos.
2. Ejecutar exportación `.neo` si corresponde.
3. Preparar runtime Piper.
4. Aplicar overrides de síntesis.
5. Delegar a servidor o síntesis CLI.

# Pendientes

- Agregar pruebas funcionales de CLI para help, version, síntesis, server y export `.neo`.
- Agregar pruebas funcionales para `.neo`.
- Agregar pruebas pequeñas de parsing de CLI sin requerir ONNX.
- Continuar después con `server/tts_scheduler.cpp` o `server/text_sanitizer.cpp`.

# Próximos pasos

Probar build Windows real con `script/build-windows.py` y validar `--help`, exportación `.neo`, síntesis WAV, `--output_raw`, `--json-input` y modo servidor.
