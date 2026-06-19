# Build Windows

Piper Neo mantiene el build local de Windows en:

```bat
py script\build-windows.py clean
py script\build-windows.py
```

El script prepara las dependencias nativas usadas por Piper: `piper-phonemize`, `espeak-ng`, ONNX Runtime, `fmt` y `spdlog`.

## Requisitos

- Windows 10/11.
- Python 3.
- CMake.
- Ninja.
- WinLibs/MinGW disponible en `PATH`.
- Espacio suficiente para `build-winlibs/`, `dist-winlibs/` y dependencias externas.

## Salida esperada

El binario final queda en:

```text
dist-winlibs\piper-neo-windows\piper.exe
```

El paquete incluye también los datos necesarios de `espeak-ng-data` y las DLL nativas requeridas.

## Smoke test recomendado

Con modelos reales:

```bat
python script\smoke-piper-binary.py --binary dist-winlibs\piper-neo-windows\piper.exe --models C:\ruta\a\piper-voices --stress-api-requests 2
```

El smoke valida:

- `--help`
- `--version`
- síntesis CLI a WAV
- arranque API en puerto libre
- `/api/health`
- `/api/v1/status`
- `/api/v1/metrics`
- `/api/v1/models`
- validación negativa de `/api/v1/tts`
- generación TTS vía API
- descarga de WAV generado
- stress concurrente opcional

## Notas

- No subas `build-winlibs/` ni `dist-winlibs/` al repo.
- Los modelos reales `.onnx`, `.onnx.json` y `.neo` deben mantenerse fuera del repo público salvo modelos de prueba explícitamente permitidos.
- Si aparece `Bad data: es_dict`, prueba con la versión que incluye protección de concurrencia eSpeak/tashkeel.
