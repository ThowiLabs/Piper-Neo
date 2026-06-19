# Fecha

19 de junio de 2026

# Objetivo

Reducir `server/routes/tts_routes.cpp` separando el parseo/validación del request JSON y el payload de respuesta TTS.

# Decisiones tomadas

- Mantener `handleTtsRoute()` como punto público de la ruta `/api/v1/tts`.
- Mover la lectura de JSON, validación de `text`, `model`, `speaker_id` y opciones float a `tts_request.*`.
- Mover el payload estándar de éxito a `tts_payload.*`.
- Mantener la decisión markup/plain dentro de `tts_routes.cpp` porque coordina scheduler, sanitizer y renderer.

# Arquitectura actual

- `tts_routes.*`: método HTTP, dispatch TTS, manejo de errores y limpieza de salida parcial.
- `tts_request.*`: validación de request JSON y construcción de `TtsRouteRequest`.
- `tts_payload.*`: respuesta JSON exitosa común para TTS plain y markup.

# Librerías usadas

Solo C++17 y componentes existentes del servidor. No se agregaron dependencias.

# Archivos importantes modificados

- `src/cpp/server/routes/tts_routes.cpp`
- `src/cpp/server/routes/tts_request.hpp`
- `src/cpp/server/routes/tts_request.cpp`
- `src/cpp/server/routes/tts_payload.hpp`
- `src/cpp/server/routes/tts_payload.cpp`
- `CMakeLists.txt`
- `script/smoke-project-structure.py`

# Problemas encontrados

`tts_routes.cpp` todavía mezclaba parseo de JSON, validación de campos, construcción de respuesta, sanitización, decisión markup/plain y manejo de errores.

# Soluciones implementadas

Se extrajo el parseo y payload común, dejando `tts_routes.cpp` como coordinador de ruta y síntesis. Esto facilita agregar pruebas de request sin levantar el servidor completo.

# Pendientes

- Agregar `test_tts_request.cpp` para validar JSON inválido, payload grande, opciones fuera de rango y aliases camelCase/snake_case.
- Validar con `smoke-piper-binary.py` después del build Windows real.

# Próximos pasos

Crear pruebas unitarias de request TTS o continuar con documentación final si el binario real sigue pasando smoke.
