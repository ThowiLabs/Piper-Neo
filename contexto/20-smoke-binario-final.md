# Fecha
2026-06-19

# Objetivo
Agregar una prueba automatizada para validar el binario final generado de Piper Neo sin intervención humana.

# Decisiones tomadas
- Crear `script/smoke-piper-binary.py` usando solo Python standard library.
- Permitir autodetección de `piper.exe` en Windows y `piper` en Linux/macOS.
- Permitir especificar `--binary` cuando el binario esté en una ruta personalizada.
- Exigir `--models` para encontrar un `.neo` o par `.onnx` + `.json` real.
- Validar CLI y API HTTP en una sola herramienta.

# Arquitectura actual
```text
script/smoke-piper-binary.py
  - resuelve binario final
  - busca modelo .neo o .onnx + .json
  - prueba --help y --version
  - genera WAV por CLI
  - levanta servidor API en puerto libre
  - prueba /api/health, /api/v1/status, /api/v1/metrics y /api/v1/models
  - prueba validación negativa de /api/v1/tts
  - genera audio por /api/v1/tts
  - descarga /api/v1/files/<wav>
  - valida encabezado RIFF/WAVE
```

# Librerías usadas
Solo Python standard library: `argparse`, `subprocess`, `urllib`, `socket`, `tempfile`, `json`, `pathlib`.

# Archivos importantes modificados
- `script/smoke-piper-binary.py`
- `README.md`
- `README.es.md`
- `docs/core-architecture.md`
- `docs/auditoria-refactor-piper-neo.md`
- `script/smoke-project-structure.py`

# Problemas encontrados
Las pruebas previas validaban estructura y módulos internos, pero no verificaban el binario final construido con dependencias reales, modelos reales y servidor HTTP ejecutándose.

# Soluciones implementadas
Se agregó smoke end-to-end configurable para binario final. Puede usarse así:

```bash
python script/smoke-piper-binary.py --models models
python script/smoke-piper-binary.py --binary dist-winlibs/piper-neo-windows/piper.exe --models models
```

# Pendientes
- Agregar casos opcionales para markup TTS en API cuando haya modelos reales disponibles.
- Agregar opción para elegir modelo específico si hay varios en `--models`.
- Agregar pruebas de token obligatorio en CI si se decide proteger la API por defecto.

# Próximos pasos
Ejecutar el smoke después de `py script\build-windows.py` en Windows con una carpeta real de modelos.
