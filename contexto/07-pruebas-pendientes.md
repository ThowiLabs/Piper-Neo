
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
cmake -S . -B build-tests -DPIPER_BUILD_TESTS=ON
cmake --build build-tests --target test_neo_package
cmake --build build-tests --target test_text_sanitizer
ctest --test-dir build-tests -R "test_neo_package|test_text_sanitizer" --output-on-failure
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
- Todavía no hay cobertura de parser HTTP ni chunking profundo. El sanitizer ya tiene una prueba C++ mínima.

# Soluciones implementadas

- Se dejaron modelos reales fuera del repo y `models/.gitkeep` como placeholder.
- Se agregaron smoke checks sin dependencias pesadas.
- Se agregó `test_neo_package`, prueba C++ opcional que crea un `.neo` mínimo, lo inspecciona, lee imagen y extrae modelo/config sin requerir ONNX real.
- Se agregó `test_text_sanitizer`, prueba C++ opcional para texto plano, URL, correo, HTML, markdown, código, emojis, texto largo e UTF-8 inválido.

# Pendientes

- Test `.neo`: ampliar cobertura a exportación zstd/no-zstd cuando zstd esté disponible.
- Test chunking: signos españoles, URLs, decimales, abreviaturas y saltos de línea.
- Test HTTP parser: GET, POST, query params, body grande y headers malformados.
- Test sanitizer: ampliar cobertura con payloads HTTP reales e inputs mixtos de usuarios.
- Test funcional con modelo real local no versionado.

# Próximos pasos

Agregar tests de chunking y HTTP parser antes de refactorizar más esos módulos; para sanitizer ya existe cobertura mínima.
