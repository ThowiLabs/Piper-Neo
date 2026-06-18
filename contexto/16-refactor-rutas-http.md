# Fecha
2026-06-18

# Objetivo
Separar el router HTTP del servidor Piper Neo para que `request_handler.cpp` deje de concentrar health/status, métricas, modelos, archivos y síntesis TTS.

# Decisiones tomadas
- Mantener `handleClient()` como punto público de entrada para no romper `server.cpp`.
- Crear `RouteContext` para pasar opciones, registry, scheduler y métricas a las rutas sin variables globales.
- Separar rutas por responsabilidad real:
  - `health_routes.*`: health, status y métricas.
  - `model_routes.*`: listado de modelos e imagen de modelo.
  - `file_routes.*`: descarga segura de WAV generados.
  - `tts_routes.*`: endpoint `/api/v1/tts`, validación JSON, sanitizer, markup y síntesis.
- Mantener autenticación, parseo HTTP y cierre de socket en `request_handler.cpp`.

# Arquitectura actual
`request_handler.cpp` queda como router mínimo. Las rutas viven en `src/cpp/server/routes/` y usan helpers existentes de HTTP, responses, model registry, sanitizer, markup y scheduler.

# Librerías usadas
No se agregaron librerías nuevas. Se mantiene C++17, spdlog, nlohmann/json vendorizado y las dependencias existentes del core.

# Archivos importantes modificados
- `src/cpp/server/request_handler.cpp`
- `src/cpp/server/http.hpp`
- `CMakeLists.txt`
- `script/smoke-project-structure.py`
- `docs/core-architecture.md`
- `docs/auditoria-refactor-piper-neo.md`
- `contexto/README.md`

# Archivos nuevos
- `src/cpp/server/routes/route_context.hpp`
- `src/cpp/server/routes/health_routes.hpp`
- `src/cpp/server/routes/health_routes.cpp`
- `src/cpp/server/routes/model_routes.hpp`
- `src/cpp/server/routes/model_routes.cpp`
- `src/cpp/server/routes/file_routes.hpp`
- `src/cpp/server/routes/file_routes.cpp`
- `src/cpp/server/routes/tts_routes.hpp`
- `src/cpp/server/routes/tts_routes.cpp`

# Problemas encontrados
`request_handler.cpp` todavía mezclaba rutas, validaciones JSON, sanitizer, markup, salida de archivos y métricas en un solo archivo. Además `http.hpp` redefinía `NOMINMAX` en MinGW cuando ya venía definido por headers del toolchain.

# Soluciones implementadas
- Se redujo `request_handler.cpp` a lectura HTTP, CORS/OPTIONS, autorización, dispatch y manejo de errores generales.
- Se movió la lógica de cada endpoint a módulos de rutas.
- Se protegió la definición de `NOMINMAX`/`WIN32_LEAN_AND_MEAN` con `#ifndef`.
- Se agregaron comprobaciones estructurales para impedir que `request_handler.cpp` vuelva a crecer.

# Pendientes
- Agregar pruebas unitarias del parser HTTP y rutas con sockets simulados o abstracción mínima de writer.
- Seguir refactorizando `tts_scheduler.cpp` si se necesita mayor separación de job queue/workers.
- Revisar `http.cpp`, que todavía concentra parser, URL decode, socket IO y envío de respuestas.

# Próximos pasos
Validar build real en Windows con `py script\\build-windows.py clean` y `py script\\build-windows.py`. Si compila, continuar con `http.cpp` o `tts_scheduler.cpp`.
