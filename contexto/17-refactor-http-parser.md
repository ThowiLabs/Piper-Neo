# Fecha
2026-06-18

# Objetivo
Separar el módulo HTTP base del servidor para que el parser de request, la E/S de socket, la generación de respuestas y el manejo de rutas/queries no vivan en un solo archivo.

# Decisiones tomadas
- Mantener `src/cpp/server/http.hpp` como contrato público compatible para no cambiar rutas ni servidor.
- Dejar `src/cpp/server/http.cpp` enfocado en lectura y parseo básico de requests HTTP.
- Mover lectura/escritura de sockets a `src/cpp/server/http/socket_io.cpp`.
- Mover respuestas HTTP, JSON y envío de archivos a `src/cpp/server/http/response_writer.cpp`.
- Mover decode de URL, query params y helpers de rutas dinámicas a `src/cpp/server/http/url.cpp`.
- Agregar `test_http_parser` para cubrir parsing de target, query params y rutas dinámicas sin depender de ONNX ni eSpeak.

# Arquitectura actual
El servidor HTTP queda dividido así:

```text
server/http.hpp                  API pública HTTP usada por rutas y request_handler.
server/http.cpp                  Lectura de request HTTP y headers.
server/http/socket_io.cpp        closeSocket, clientDisconnected, recvAppend y sendRaw.
server/http/response_writer.cpp  sendResponse, sendJson y sendFile.
server/http/url.cpp              parseTarget, queryValue, routeFileName y routeModelImageName.
```

# Librerías usadas
- C++17 standard library.
- Sockets nativos de Windows/POSIX ya usados por el proyecto.
- `json.hpp` existente para respuestas JSON.

# Archivos importantes modificados
- `CMakeLists.txt`
- `src/cpp/server/http.cpp`
- `src/cpp/server/http/response_writer.cpp`
- `src/cpp/server/http/socket_io.cpp`
- `src/cpp/server/http/url.cpp`
- `src/cpp/tests/test_http_parser.cpp`
- `script/smoke-project-structure.py`
- `docs/core-architecture.md`
- `docs/auditoria-refactor-piper-neo.md`
- `contexto/README.md`

# Problemas encontrados
`http.cpp` todavía era un punto de mezcla entre socket I/O, parser, URL helpers, respuestas y archivos. Eso aumentaba el riesgo de colisiones de símbolos como el fallo anterior de `multiple definition`.

# Soluciones implementadas
- Separación por responsabilidades reales sin cambiar la API externa.
- Nuevo test de parsing de URL/query y rutas dinámicas.
- Smoke estructural actualizado para evitar regresiones.

# Pendientes
- Agregar pruebas de lectura HTTP completa con socket simulado o función parser pura.
- Seguir con `tts_scheduler.cpp`, `model_cache.cpp` y `model_registry.cpp`.

# Próximos pasos
Refactorizar la cola y estado de jobs del scheduler o dividir catálogo/cache de modelos.
