# Desarrollo de Piper Neo

Este documento resume la forma recomendada de validar cambios antes de hacer commit o release.

## Alcance del repo público

Piper Neo contiene solo el motor: CLI C++, servidor HTTP local, empaquetado `.neo`, normalización por modelo, scripts de build/smoke y documentación. La carpeta `apps/` no forma parte del repo público del motor.

## Build local Windows

```bat
py script\build-windows.py clean
py script\build-windows.py
```

No modifiques `script/build-windows.py` salvo que el fallo esté directamente en ese script.

## Smoke estructural

```bat
python script\smoke-project-structure.py
python script\smoke-text-normalizer.py
```

## Smoke del binario final

Después del build, ejecuta:

```bat
python script\smoke-piper-binary.py --binary dist-winlibs\piper-neo-windows\piper.exe --models C:\ruta\a\piper-voices --stress-api-requests 2
```

Este smoke valida:

- `--help` y `--version`.
- CLI `--text` + `--output_file`.
- CLI `--input_file`.
- stdin plano.
- JSON lines con `--json-input`.
- `--output_dir`.
- `--output_raw`.
- API HTTP `/api/health`, `/api/v1/status`, `/api/v1/metrics`, `/api/v1/models`, `/api/v1/tts` y descarga de WAV.
- Stress concurrente para detectar regresiones de eSpeak/model cache.

Si `--output_raw` falla en un entorno específico, puede omitirse temporalmente con:

```bat
python script\smoke-piper-binary.py --models C:\ruta\a\piper-voices --skip-cli-raw
```

## Normalización de texto

El core no convierte números, moneda, porcentajes ni versiones. Eso debe configurarse por modelo mediante `neo.text_normalization.replacements` en el JSON. Los builtins del core se limitan a URLs/correos cuando el modelo los activa.

## Workflows

- `Build Piper Neo`: automático en `main`, no crea release.
- `Build Release Piper Neo`: manual, crea o actualiza GitHub Releases.
