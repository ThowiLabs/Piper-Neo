
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
script/smoke-piper-binary.py --models <ruta-modelos>
cmake -S . -B build -DPIPER_BUILD_TESTS=OFF
cmake -S . -B build-tests -DPIPER_BUILD_TESTS=ON
cmake --build build-tests --target test_neo_package test_text_sanitizer test_http_parser test_text_chunker test_resource_policy
ctest --test-dir build-tests -R "test_neo_package|test_text_sanitizer|test_http_parser|test_text_chunker|test_resource_policy" --output-on-failure
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
- `src/cpp/tests/test_neo_package.cpp`
- `src/cpp/tests/test_text_sanitizer.cpp`
- `CMakeLists.txt`

# Problemas encontrados

- La síntesis real depende de modelos externos que no deben subirse al repo.
- Antes no había una prueba funcional de paquetes `.neo`.
- Ya existe cobertura mínima para `.neo`, sanitizer, parser HTTP, chunking y límites puros de recursos. Falta integración real de servidor y audio con modelos locales.

# Soluciones implementadas

- Se dejaron modelos reales fuera del repo y `models/.gitkeep` como placeholder.
- Se agregaron smoke checks sin dependencias pesadas.
- Se agregó `test_neo_package`, prueba C++ opcional que crea un `.neo` mínimo, lo inspecciona, lee imagen y extrae modelo/config sin requerir ONNX real.
- Se agregó `test_text_sanitizer`, prueba C++ opcional para texto plano, URL, correo, HTML, markdown, código, emojis, texto largo e UTF-8 inválido.
- Se agregó `test_http_parser` para URL/query/rutas.
- Se agregó `test_text_chunker` para límites de oración, signos españoles, UTF-8 y palabras largas.
- Se agregó `test_resource_policy` para límites puros de memoria temporal y réplicas.

# Pendientes

- Test `.neo`: ampliar cobertura a exportación zstd/no-zstd cuando zstd esté disponible.
- Test chunking: ampliar con URLs, correos, decimales, abreviaturas reales y saltos de línea complejos.
- Test HTTP parser: ampliar con POST completo, body grande y headers malformados.
- Test sanitizer: ampliar cobertura con payloads HTTP reales e inputs mixtos de usuarios.
- Test funcional con modelo real local no versionado.

# Próximos pasos

Agregar pruebas de integración HTTP real y audio real con modelos locales antes de partir `synthesis_pipeline.cpp`.
