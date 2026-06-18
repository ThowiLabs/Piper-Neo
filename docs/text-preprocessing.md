# Filtro y sanitizado de texto para TTS

Piper Neo protege el texto antes de enviarlo al fonemizador y al modelo de voz. La meta no es censurar contenido, sino convertir entradas ruidosas en texto pronunciable para evitar balbuceos, cortes raros o lecturas ininteligibles.

## Capas implementadas

1. **Servidor C++ `/api/v1/tts`**
   - Aplica una capa obligatoria para proteger cualquier cliente externo.
   - Valida UTF-8 de forma estricta.
   - Elimina controles, caracteres invisibles, BOM, variation selectors y marcas de dirección.
   - Normaliza puntuación común, full-width ASCII y ligaduras frecuentes.
   - Convierte HTML/BBCode/Markdown a texto plano.
   - Ya no resume URLs ni correos; solo protege/limpia ruido técnico, hashes/base64, código y emoji. La lectura de URLs/correos queda en la configuración del modelo.
   - Colapsa repeticiones y espacios.
   - Rechaza con `422 text_not_pronounceable` si el texto queda vacío o no es seguro.

2. **Normalización por modelo**
   - Si el modelo declara `neo.text_normalization`, se aplican reglas de lectura para decimales, moneda, porcentajes, versiones, URLs, correos y reemplazos.
   - Los modelos clásicos sin configuración Neo conservan el comportamiento original.

## Respuesta de la API

`POST /api/v1/tts` agrega el campo opcional `text_preprocessing`:

```json
{
  "text_preprocessing": {
    "speakText": "Texto final enviado al motor de voz",
    "warnings": ["MARKUP_STRIPPED"],
    "riskScore": 0.28,
    "stats": {
      "rawChars": 120,
      "speakChars": 84,
      "rawBytes": 130,
      "speakBytes": 88,
      "urls": 1,
      "emails": 0,
      "codeBlocks": 0,
      "emojis": 0
    }
  }
}
```

## Métricas

`GET /api/v1/metrics` incluye:

```json
{
  "text_preprocessing": {
    "sanitized_inputs": 0,
    "sanitize_warnings": 0,
    "rejected_inputs": 0
  }
}
```

Estas métricas permiten detectar entradas problemáticas sin guardar texto crudo de los usuarios.

## Implementación interna

La API pública del sanitizer vive en:

```text
src/cpp/server/text_sanitizer.hpp
src/cpp/server/text_sanitizer.cpp
```

La lógica interna está separada para evitar que un solo archivo vuelva a mezclar todas las reglas:

```text
src/cpp/server/sanitize_result.*       Resultado, métricas y warnings únicos.
src/cpp/server/sanitize/utf8_text.*    UTF-8 estricto, Unicode, emojis, whitespace y recorte.
src/cpp/server/sanitize/content_filters.* HTML, BBCode, markdown, código, alta entropía, URLs y correos.
src/cpp/server/sanitize/risk_score.*   Cálculo de riesgo por warnings.
```

## Prueba funcional

El sanitizer tiene una prueba C++ opcional que no requiere modelos ONNX:

```bash
cmake -S . -B build-tests -DPIPER_BUILD_TESTS=ON
cmake --build build-tests --target test_text_sanitizer
ctest --test-dir build-tests -R test_text_sanitizer --output-on-failure
```

Cubre texto plano, URLs, correos, HTML, markdown, bloques de código, emojis, texto largo e UTF-8 inválido.
