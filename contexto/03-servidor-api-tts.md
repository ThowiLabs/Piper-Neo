
# Fecha

18 de junio de 2026

# Objetivo

Documentar el servidor HTTP local de Piper Neo y sus límites operativos.

# Decisiones tomadas

- El servidor se mantiene dentro del mismo binario `piper`.
- `server.cpp` gestiona runtime, sockets y aceptación de conexiones.
- `request_handler.*` enruta endpoints y valida requests.
- La autenticación por token es opcional.
- Los archivos generados se sirven desde nombres seguros y se limpian por retención.

# Arquitectura actual

```text
server.cpp               runtime socket y recursos
server/http.*            parser HTTP, query params, socket IO y archivos
server/auth.*            Bearer token y X-API-Token
server/request_handler.* rutas y validación por request
server/text_sanitizer.*  orquestador de limpieza segura antes de TTS
server/sanitize_result.* resultado de sanitización y warnings
server/sanitize/*       UTF-8, filtros de contenido y risk score
server/model_registry.*  listado y metadata de modelos
server/model_cache.*     carga y leases de voces
server/tts_scheduler.*   cola justa y concurrencia
server/markup_tts.*      scripts multi-segmento/multi-modelo
server/output_cleanup.*  limpieza de temporales y outputs
```

# Librerías usadas

- C++17.
- fmt.
- spdlog.
- piper-phonemize.
- espeak-ng.
- ONNX Runtime.
- zstd opcional para paquetes `.neo`.
- nlohmann/json como header vendorizado.

# Archivos importantes modificados

- `src/cpp/server.cpp`
- `src/cpp/server/*`
- `docs/api-server.md`
- `docs/markup-tts.md`
- `docs/text-preprocessing.md`

# Problemas encontrados

- El servidor original mezclaba HTTP, modelos, sanitización, scheduler y salida de archivos.
- Existía riesgo de duplicar funciones HTTP transversales si no se mantenía un límite claro.

# Soluciones implementadas

- Se centralizaron funciones HTTP en `server/http.*`.
- Se dejaron utilidades generales en `server/utils.*`.
- Se mantuvo separación de auth, responses, modelos, cache, scheduler y limpieza.
- El sanitizer se separó en módulos internos bajo `server/sanitize/` para reducir acoplamiento y facilitar pruebas.

# Pendientes

- Agregar pruebas HTTP de `/api/health`, `/api/v1/models`, `/api/v1/tts` y descarga de archivos.
- Revisar límites de payload con inputs extremos.

# Próximos pasos

Crear smoke tests HTTP que arranquen el binario compilado y validen respuestas mínimas.
