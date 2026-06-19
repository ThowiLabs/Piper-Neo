# Fecha

19 de junio de 2026

# Objetivo

Cerrar la refactorización del motor Piper Neo con una pasada final de bajo riesgo: plataforma, smoke CLI avanzado, auditoría de residuos y documentación final.

# Decisiones tomadas

- `platform.cpp` queda como fachada mínima; la implementación real se separa por responsabilidad.
- `platform_console.cpp` concentra la configuración UTF-8 de consola en Windows.
- `platform_paths.cpp` concentra la resolución multiplataforma de la ruta del ejecutable.
- `smoke-piper-binary.py` valida más modos reales del binario final, no solo API.
- La conversión de números, moneda, porcentajes y versiones no pertenece al core; se delega a replacements por modelo.

# Arquitectura actual

El cierre mantiene la API pública estable. El CLI se valida con:

- `--text` + `--output_file`.
- `--input_file` + `--output_file`.
- stdin plano + `--output_file`.
- `--json-input` por línea con `output_file`.
- `--output_dir` con nombre generado.
- `--output_raw` como audio PCM crudo.

# Librerías usadas

Python estándar para smoke checks y C++17 para el motor.

# Archivos importantes modificados

- `src/cpp/app/platform.cpp`
- `src/cpp/app/platform_console.cpp`
- `src/cpp/app/platform_paths.cpp`
- `script/smoke-piper-binary.py`
- `script/smoke-project-structure.py`
- `docs/text-normalization.md`
- `docs/development.md`
- `README.md`
- `README.es.md`

# Problemas encontrados

Quedaban referencias documentales ambiguas a normalización de moneda, porcentajes y versiones como si fueran conversiones builtin del core.

# Soluciones implementadas

Se aclaró que el core solo conserva builtins seguros para URLs/correos cuando el modelo los activa; todo lo demás debe vivir en `neo.text_normalization.replacements` del JSON del modelo.

# Pendientes

Validar el ZIP en Windows con `py script\build-windows.py clean`, `py script\build-windows.py` y `python script\smoke-piper-binary.py --models <ruta> --stress-api-requests 2`.

# Próximos pasos

Si el build y smoke pasan, la refactorización del motor puede considerarse cerrada para commit/release.
