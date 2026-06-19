# API HTTP de Piper Neo

El servidor se inicia con:

```bat
piper.exe --server --host 127.0.0.1 --port 8080 --models C:\ruta\a\piper-voices
```

## Seguridad

Si no se configura token, la API queda abierta en el host enlazado. Para uso fuera de localhost, usa token:

```bat
piper.exe --server --api-token TU_TOKEN --models C:\ruta\a\piper-voices
```

Enviar token:

```http
Authorization: Bearer TU_TOKEN
```

## Endpoints principales

### `GET /api/health`

Estado básico del servidor.

### `GET /api/v1/status`

Estado detallado de servidor, recursos y configuración segura.

### `GET /api/v1/metrics`

Métricas de jobs, sanitizer, cola y recursos.

### `GET /api/v1/models`

Lista de modelos detectados desde `--models`.

### `GET /api/v1/models/<modelo>/image`

Devuelve imagen embebida de model card si existe.

### `POST /api/v1/tts`

Genera audio WAV.

Payload mínimo:

```json
{
  "text": "Hola desde Piper Neo",
  "model": "es_MX-voz.onnx"
}
```

Campos opcionales:

```json
{
  "speaker_id": 0,
  "noise_scale": 0.667,
  "length_scale": 1.0,
  "noise_w": 0.8,
  "sentence_silence_seconds": 0.2
}
```

Respuesta exitosa: JSON con nombre de archivo y URL descargable.

### `GET /api/v1/files/<archivo.wav>`

Descarga WAV generado. El servidor valida nombre seguro para evitar path traversal.

## Markup TTS

`text` puede incluir markup compatible con Piper Neo para combinar segmentos/modelos/silencios. La detección y render se manejan en `server/markup/`.

## Validaciones

La API aplica:

- límite `--max-input-bytes`
- sanitizer de texto
- validación JSON
- rechazo de `output_file` manual
- nombres de salida seguros
- limpieza de temporales por retención
