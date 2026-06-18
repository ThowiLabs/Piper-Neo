
# Fecha

18 de junio de 2026

# Objetivo

Registrar pruebas mínimas que faltan para seguir refactorizando sin romper comportamiento.

# Decisiones tomadas

- No seguir partiendo lógica delicada sin pruebas.
- Priorizar `.neo`, chunking, HTTP parser, sanitizer y síntesis real.
- Mantener smoke tests ligeros ejecutables sin modelos pesados.

# Arquitectura actual

Pruebas actuales:

```text
script/smoke-text-normalizer.py
script/smoke-project-structure.py
cmake -S . -B build -DPIPER_BUILD_TESTS=OFF
```

Pruebas futuras deberían vivir en `script/` o como tests C++ opcionales.

# Librerías usadas

- Python 3 para smoke scripts.
- Compilador C++17 para smoke de normalización.
- CMake para validación de build.

# Archivos importantes modificados

- `script/smoke-text-normalizer.py`
- `script/smoke-project-structure.py`
- `src/cpp/test.cpp`

# Problemas encontrados

- La síntesis real depende de modelos externos que no deben subirse al repo.
- Las pruebas actuales no cubren paquetes `.neo`, parser HTTP ni chunking profundo.

# Soluciones implementadas

- Se dejaron modelos reales fuera del repo y `models/.gitkeep` como placeholder.
- Se agregaron smoke checks sin dependencias pesadas.

# Pendientes

- Test `.neo`: exportar, inspeccionar, extraer y validar metadata.
- Test chunking: signos españoles, URLs, decimales, abreviaturas y saltos de línea.
- Test HTTP parser: GET, POST, query params, body grande y headers malformados.
- Test sanitizer: texto normal, código, URLs largas, emojis, input vacío e input enorme.
- Test funcional con modelo real local no versionado.

# Próximos pasos

Agregar tests de `.neo` y chunking antes del siguiente refactor grande.
