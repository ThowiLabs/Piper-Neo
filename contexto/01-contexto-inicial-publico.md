
# Fecha

18 de junio de 2026

# Objetivo

Documentar el estado inicial del repo público limpio de Piper Neo.

# Decisiones tomadas

- Piper Neo se presenta como una versión de `rhasspy/piper` con mejoras prácticas para CLI, servidor local, paquetes `.neo`, textos largos y control de recursos.
- El repositorio queda sin `apps/`.
- Se eliminan referencias a clientes de escritorio externos.
- Se conservan utilidades de entrenamiento/runtime heredadas de Piper porque forman parte del ecosistema del motor.
- Se conserva `script/build-windows.py` para pruebas locales de Windows.

# Arquitectura actual

```text
src/cpp/          motor C++, CLI, servidor HTTP y paquetes .neo
src/python/       utilidades de entrenamiento
src/python_run/   utilidades runtime Python
script/           build y smoke checks
docs/             documentación técnica
contexto/         contexto técnico actual
neo-docs/         formato .neo
models/           placeholder para voces locales
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

- `README.md`
- `README.es.md`
- `.gitignore`
- `.github/workflows/main.yml`
- `contexto/*`

# Problemas encontrados

- El repo todavía contenía `apps/` y referencias públicas a clientes externos.
- El contexto mezclaba historial viejo y errores ya corregidos.
- `.gitignore` tenía reglas para apps ya eliminadas.

# Soluciones implementadas

- Se eliminó `apps/`.
- Se reescribieron README y contexto para el repo público del motor.
- Se limpiaron reglas residuales de `.gitignore`.
- Se ajustó el workflow para operar desde `main`.

# Pendientes

- Validar build local con `script/build-windows.py`.
- Validar workflow manual en GitHub después de subir el repo.
- Probar síntesis real con modelos `.onnx` y `.neo`.

# Próximos pasos

Crear el repo público en GitHub, subir esta base inicial y ejecutar smoke checks.
