# Fecha

19 de junio de 2026

# Objetivo

Corregir un fallo de compilación introducido durante la separación de utilidades multimedia del servidor.

# Decisiones tomadas

- Mantener `parseDataImage()` dentro de `src/cpp/server/media/data_image.*`.
- No regresar lógica multimedia a `server/utils.*`.
- Corregir únicamente el include faltante en la ruta de modelos.
- Agregar una validación estructural para evitar que la ruta vuelva a usar `parseDataImage()` sin incluir su header.

# Arquitectura actual

- `server/routes/model_routes.cpp` atiende `/api/v1/models` y `/api/v1/models/<modelo>/image`.
- `server/media/data_image.*` parsea `data:image/...;base64,...` y devuelve MIME + bytes.
- `server/media/base64.*` decodifica Base64.
- `server/utils.*` queda libre de helpers multimedia específicos.

# Librerías usadas

C++17 y utilidades internas del servidor. No se agregaron dependencias.

# Archivos importantes modificados

- `src/cpp/server/routes/model_routes.cpp`
- `script/smoke-project-structure.py`
- `contexto/README.md`
- `contexto/28-correccion-build-data-image-route.md`

# Problemas encontrados

El build de Windows fallaba en `src/cpp/server/routes/model_routes.cpp` porque el archivo llamaba `parseDataImage()` pero no incluía `server/media/data_image.hpp`.

# Soluciones implementadas

Se agregó el include correcto en `model_routes.cpp`:

```cpp
#include "server/media/data_image.hpp"
```

También se actualizó el smoke estructural para detectar ese acoplamiento explícito.

# Pendientes

Ejecutar build real de Windows y smoke binario completo con modelos reales.

# Próximos pasos

Después de confirmar build, continuar con pruebas/fakes de `model_cache`/`model_registry` y revisión final de documentación.
