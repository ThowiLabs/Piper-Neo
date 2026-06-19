
# Fecha

18 de junio de 2026

# Objetivo

Documentar la normalización de texto configurable por modelo.

# Decisiones tomadas

- Los modelos clásicos sin `neo.text_normalization` conservan comportamiento original.
- Los modelos Neo pueden activar reglas para decimales, moneda, porcentajes, versiones, URLs, correos y reemplazos.
- La normalización se mantiene separada del sanitizer del servidor.

# Arquitectura actual

```text
src/cpp/text_normalizer.cpp       API pública parse/normalize
src/cpp/text/string_utils.*       utilidades de strings y límites
src/cpp/text/replacements.*       reemplazos configurables
src/cpp/text/builtin_normalizer.* reglas builtin protegidas
```

# Librerías usadas

- C++17.
- nlohmann/json vendorizado.

# Archivos importantes modificados

- `src/cpp/text_normalizer.cpp`
- `src/cpp/text_normalizer.hpp`
- `src/cpp/text/*`
- `docs/text-normalization.md`
- `script/smoke-text-normalizer.py`

# Problemas encontrados

- Mezclar sanitización, reemplazos y reglas builtin en un solo archivo hacía difícil probar cambios.
- Cambiar reglas podía romper modelos clásicos si no se protegía compatibilidad.

# Soluciones implementadas

- Se separaron módulos internos.
- Se agregó smoke test para JSON clásico, replacements legacy y builtins protegidos.
- Se documentó que la normalización no se activa sin configuración Neo o legacy.

# Pendientes

- Ampliar pruebas para URLs, correos, moneda mexicana, porcentajes, versiones y reemplazos por modelo.
- Probar pronunciación real con voces en español mexicano.

# Próximos pasos

Agregar más casos a `script/smoke-text-normalizer.py` antes de tocar reglas nuevas.
