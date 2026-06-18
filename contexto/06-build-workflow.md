
# Fecha

18 de junio de 2026

# Objetivo

Documentar build local y workflow de GitHub para repo público.

# Decisiones tomadas

- El script local `script/build-windows.py` se conserva.
- El workflow `.github/workflows/main.yml` usa explícitamente la rama `main`.
- El workflow genera release manual de Windows amd64.
- Los builds locales, distros, DLLs, outputs y modelos reales se ignoran.

# Arquitectura actual

```text
script/build-windows.py          build local Windows
.github/workflows/main.yml       release manual desde main
CMakeLists.txt                   build C++
Dockerfile.cli                   imagen CLI/servidor Linux
docker-compose-cli.yml           compose para servidor local
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

- `.github/workflows/main.yml`
- `.gitignore`
- `CMakeLists.txt`
- `script/build-windows.py`

# Problemas encontrados

- El repo público necesitaba evitar referencias a ramas antiguas para releases.
- `.gitignore` tenía reglas de apps eliminadas.

# Soluciones implementadas

- El workflow ahora hace checkout explícito de `main` y publica releases apuntando al commit de `main`.
- Se limpiaron reglas de apps eliminadas en `.gitignore`.
- Se conservaron reglas para builds, caches, modelos y outputs locales.

# Pendientes

- Probar el workflow en GitHub después del primer push.
- Confirmar que vcpkg instala zstd correctamente en `windows-latest`.

# Próximos pasos

Subir repo a GitHub, ejecutar workflow manual y validar asset `piper_windows_amd64.zip`.
