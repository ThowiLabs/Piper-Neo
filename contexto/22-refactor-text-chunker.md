# Fecha

19 de junio de 2026

# Objetivo

Separar el particionado inteligente de texto largo para que `text_chunker.cpp` deje de concentrar reglas UTF-8, signos españoles, límites de oración y fallback por tamaño.

# Decisiones tomadas

- Mantener `piper::splitTextIntoChunks()` como API pública estable.
- Mover helpers UTF-8 y whitespace a `src/cpp/core/text/utf8_utils.*`.
- Mover reglas de selección de punto de corte a `src/cpp/core/text/chunk_rules.*`.
- Agregar prueba unitaria `test_text_chunker` para validar límites de oración, signos españoles, UTF-8 y palabras largas.
- No tocar `synthesis_pipeline.cpp` todavía porque afecta audio real y requiere pruebas con modelos.

# Arquitectura actual

- `src/cpp/core/text_chunker.cpp`: orquestador público de chunks.
- `src/cpp/core/text/utf8_utils.*`: límites seguros UTF-8, whitespace y prefijos binarios.
- `src/cpp/core/text/chunk_rules.*`: reglas de corte por párrafo, oración, signos españoles, whitespace y hard limit.
- `src/cpp/tests/test_text_chunker.cpp`: prueba mínima de comportamiento de chunking sin dependencias ONNX.

# Librerías usadas

Solo C++17 y librería estándar.

# Archivos importantes modificados

- `src/cpp/core/text_chunker.cpp`
- `src/cpp/core/text/utf8_utils.hpp`
- `src/cpp/core/text/utf8_utils.cpp`
- `src/cpp/core/text/chunk_rules.hpp`
- `src/cpp/core/text/chunk_rules.cpp`
- `src/cpp/tests/test_text_chunker.cpp`
- `CMakeLists.txt`
- `script/smoke-project-structure.py`

# Problemas encontrados

`text_chunker.cpp` todavía tenía más de 300 líneas con varias responsabilidades internas. Esto complicaba agregar pruebas específicas para textos largos y aumentaba el riesgo de romper cortes UTF-8 o signos de interrogación/exclamación en español.

# Soluciones implementadas

Se extrajeron helpers y reglas a módulos internos, dejando `text_chunker.cpp` como fachada de particionado. Se agregó `test_text_chunker` para validar reconstrucción exacta del texto original y seguridad básica de bordes UTF-8.

# Pendientes

- Ampliar pruebas con URLs, correos, decimales y abreviaturas reales.
- Validar audio real con textos largos antes de modificar `synthesis_pipeline.cpp`.
- Revisar si conviene exponer métricas de chunking para debug del servidor.

# Próximos pasos

Probar build Windows y ejecutar `test_text_chunker` junto con los smoke tests existentes.
