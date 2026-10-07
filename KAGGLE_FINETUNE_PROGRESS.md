# Piper-Neo Kaggle Fine-tune — Bitácora reproducible

> Objetivo: corregir el flujo de fine-tune desde checkpoint base, crear una interfaz Gradio persistente y dejar un proceso reproducible/resiliente para Kaggle.

## Principios del entorno

- Workdir persistente de sesión: `/kaggle/working`
- Repositorio: `/kaggle/working/Piper-Neo`
- Dataset de trabajo: `/kaggle/working/piper_data/capibara`
- El entrenamiento debe ejecutarse en Python 3.10 aislado, no en el Python 3.13 del notebook.
- La interfaz final deberá aceptar:
  - URL pública de Google Drive
  - enlace HTTP(S) directo
  - ZIP subido manualmente
  - CSV/metadata subido manualmente
- Debe poder:
  - iniciar fine-tune desde checkpoint base sin heredar epoch/global_step del modelo origen
  - reanudar un entrenamiento propio cuando se elija explícitamente
  - guardar checkpoints periódicos
  - conservar `config.json`
  - descargar checkpoints desde Gradio
  - opcionalmente subir checkpoints + config a Hugging Face con sus nombres originales
  - sobrevivir a la muerte del notebook mediante sincronización periódica externa

## Estado confirmado

### Git
- Rama: `main`
- Commit base al iniciar trabajo: `b80918f`
- Autor del último commit: `YahirHub <291061271+ThowiLabs@users.noreply.github.com>`
- No se cambiará la identidad del autor.
- El commit final se hará únicamente después de validar el flujo.

### Runtime Kaggle
- Python global: 3.13.15
- GPU visible en el runtime actual: NO
- `nvidia-smi`: no disponible
- Esto impide hacer una validación GPU completa en esta sesión, pero sí permite validar instalación, dataset, carga de checkpoint y flujo CPU/smoke test.

### Entorno de training creado
- Python: 3.10.21 mediante `uv`
- venv: `src/python/.venv`
- PyTorch: 1.13.1+cu117
- pytorch-lightning: 1.9.5
- torchmetrics: 0.11.4
- piper-phonemize: 1.1.0
- Gradio: 4.44.1
- huggingface_hub: 0.25.2
- gdown: 5.2.0

### Corrección de dependencia detectada
`pytorch-lightning==1.9.5` falla con setuptools moderno porque `pkg_resources` ya no está disponible como espera Lightning 1.x.

Se fijó:
- `setuptools==70.3.0`

Con esto:
- Lightning importa correctamente.
- `build_monotonic_align.sh` compila correctamente.

### Dataset Capibara
- CSV original: 4000 registros lógicos.
- WAV únicos encontrados en el ZIP: 3999.
- Inconsistencia detectada:
  - fila 1675: `1675.wav|Hola, te envié un clip muy bueno de Twitch.`
  - fila 1676: `1675.wav|Oye, vi que publicaste una oferta de trabajo en LinkedIn.`
  - falta físicamente `1676.wav`.
- No se inventará audio. El servicio deberá validar y reportar filas duplicadas/archivos faltantes antes del preprocess.
- Estructura preparada:
  - `input/metadata.csv`
  - `input/wav/*.wav`

### Idioma
- Dataset objetivo: español latinoamericano/México.
- Tag de fonemización elegido: `es-419`.
- Checkpoint base solicitado: `rhasspy/piper-checkpoints/es/es_ES/davefx/medium`
- El checkpoint base es single-speaker, 22050 Hz, 256 símbolos.

### Checkpoint base
Archivo descargado:
`epoch=5629-step=1605020.ckpt`

Metadatos:
- epoch: 5629
- global_step: 1605020
- num_symbols: 256
- num_speakers: 1
- sample_rate: 22050

## Bug conceptual identificado en fine-tune

El flujo existente documenta `--resume_from_checkpoint` para fine-tune. Esto restaura no solo pesos, sino también:
- epoch
- global_step
- optimizadores
- schedulers
- estado interno de Lightning

Eso es correcto para **reanudar** una corrida interrumpida, pero no para **fine-tune desde un modelo base**.

La corrección será separar explícitamente:
1. **Fine-tune / init-from-checkpoint**: cargar solo pesos compatibles y comenzar una nueva corrida desde epoch 0.
2. **Resume**: restaurar estado completo de una corrida propia.

## Próximos pasos

- [ ] confirmar soporte real de `es-419` en piper-phonemize
- [ ] implementar modo `--init-from-checkpoint`
- [ ] validar carga de pesos contra davefx-medium
- [ ] añadir validación estricta del dataset antes del preprocess
- [ ] corregir requirements/instalador reproducible
- [ ] crear backend de ingestión Drive/direct URL/ZIP
- [ ] crear Gradio
- [ ] añadir guardado periódico + sincronización Hugging Face
- [x] smoke test CPU
- [ ] test GPU cuando el runtime tenga GPU habilitada
- [ ] commit preservando autor Git


## Avances confirmados adicionales

### es-419
Se probó directamente con piper-phonemize 1.1.0:
- `phonemize_espeak(..., "es-419")` funciona correctamente.
- Se mantiene `es-419` como idioma predeterminado para el dataset mexicano/latinoamericano.

### Fine-tune real desde davefx
Se implementó `--init-from-checkpoint`.
Prueba contra el checkpoint real de davefx-medium:
- tensores del modelo actual: 784
- tensores compatibles cargados: 784/784
- shape mismatches: 0
- checkpoint origen: epoch 5629 / global_step 1605020
- el nuevo modo NO restaura epoch/global_step/optimizer/scheduler.

### Dataset real
La validación web se ejecutó con el ZIP y CSV reales:
- WAV únicos: 3999
- filas metadata: 4000
- filas utilizables: 3999
- duplicados descartados: 1
- duplicado: línea 1676 -> 1675.wav
- WAV faltantes referenciados por metadata ya normalizada: 0

### Gradio
Implementado en:
`src/python/piper_train/gradio_finetune.py`

Incluye:
- Google Drive público
- URL HTTP(S) directa
- ZIP subido
- metadata CSV/TXT
- validación y normalización
- preprocess Piper
- Fine-tune separado de Resume
- URL/upload para modelo base
- URL/upload/último local para Resume
- checkpoints descargables
- config.json descargable
- backup Hugging Face
- preservación del nombre original del checkpoint
- sincronización periódica
- protección para no subir un checkpoint mientras todavía se escribe
- uploads de hasta 10 GB en Gradio
- paths de recuperación permitidos para descarga

### Checkpoints resilientes
El trainer ahora admite:
- `--checkpoint-epochs N`
- `--checkpoint-minutes N`

Si se usa `--checkpoint-minutes`, tiene prioridad y guarda por tiempo de entrenamiento.
Los checkpoints usan:
- `save_top_k=1`
- no se genera una copia duplicada `last.ckpt`

Se conserva localmente solo el checkpoint numerado más reciente. El backup de Hugging Face se ejecuta con un intervalo menor al intervalo de checkpoint para subirlo antes de que el siguiente lo reemplace.
Esto evita depender de que termine una época larga antes de tener recuperación y reduce mucho el riesgo de llenar el disco de Kaggle.

### Instalación reproducible
Script:
`script/setup_kaggle_finetune.sh`

Comando web instalado:
`piper-finetune-web`

### Cuaderno Kaggle
Creado:
`notebooks/kaggle_finetune_gradio.ipynb`

El notebook quedó fijado al commit validado `dac2511eb86ad61c18e32f627baa40e72da776a9` para que el bootstrap no dependa de cambios futuros en `main`.


## Validación real de fine-tune

- Preprocess real completado con código de salida 0.
- Dataset final procesado: 3999 utterances válidas.
- Se ejecutó un smoke test de entrenamiento real.
- Checkpoint generado: `epoch=0-step=2.ckpt`.
- Metadatos del checkpoint de prueba:
  - epoch: 0
  - global_step: 2
  - pytorch-lightning: 1.9.5
  - optimizer_states: 2
  - state_dict: 784 tensores
- Comparación contra `davefx-medium`:
  - 784/784 tensores flotantes cambiaron después de los dos pasos.
  - Esto confirma que se ejecutó backprop/optimizer step y no solo carga de pesos.
- Uso de disco llegó a 93% durante pruebas.
- Se eliminaron artefactos redundantes generados durante la preparación y se recuperó espacio:
  - estado posterior: 78% usado, ~4.5 GB libres.
- Cada checkpoint completo pesa aproximadamente 846 MB.
- Se ajustó ModelCheckpoint para conservar solo el checkpoint numerado más reciente, sin duplicarlo como `last.ckpt`, evitando crecimiento local sin límite.
- Recomendación operativa: mantener el intervalo de sync a Hugging Face menor que el intervalo de checkpoint (por defecto 2 min vs 15 min), para subir cada checkpoint antes de que el siguiente lo reemplace localmente.


## Cierre de validación

### Servicio web
Se detectaron y corrigieron incompatibilidades ajenas al código de Piper:
- Gradio 4.44.1 + Pydantic 2.13.x: fallo de JSON Schema.
- Gradio 4.44.1 + FastAPI/Starlette 2026: fallo de TemplateResponse.

Stack web fijado:
- gradio 4.44.1
- fastapi 0.112.2
- starlette 0.38.6
- pydantic 2.9.2
- huggingface_hub 0.25.2
- gdown 5.2.0

Prueba HTTP real:
- servidor local iniciado con piper-finetune-web
- GET http://127.0.0.1:7861 -> HTTP 200, text/html

### Checkpoint por tiempo
Prueba real con --checkpoint-minutes 0.001 y max_steps=1:
- fine-tune inicializó 784/784 tensores desde davefx-medium
- checkpoint generado: epoch=0-step=2.ckpt
- proceso finalizó con código 0
- no se genera last.ckpt duplicado
- se conserva un único checkpoint local reciente (save_top_k=1)

Comparación real contra el checkpoint base:
- 784 tensores flotantes comparados
- 784 tensores cambiaron después del smoke training

### Proyecto listo
Proyecto preparado para usar desde Gradio:
`/kaggle/working/piper_finetune/capibara`

Validación:
- dataset: capibara
- idioma: es-419
- sample rate: 22050
- num_symbols: 256
- num_speakers: 1
- utterances: 3999
- rutas de audio/cache verificadas en muestras distribuidas: OK
- checkpoint base: epoch=5629-step=1605020.ckpt


## Git y servidor final

Commits realizados con el autor persistente del repositorio:
- dac2511eb86ad61c18e32f627baa40e72da776a9 — Fix fine-tune and add resilient Kaggle Gradio service
- b2b8076130be530fa558b19ae3122ec85ae543e6 — Pin Kaggle fine-tune notebook to validated revision

Identidad Git local persistente:
- YahirHub <291061271+ThowiLabs@users.noreply.github.com>

El notebook está fijado al commit funcional dac2511eb86ad61c18e32f627baa40e72da776a9, por lo que su bootstrap no depende de cambios futuros en main.

Validación de servidor:
- servidor local: OK / HTTP 200
- túnel público Gradio: OK
- Gradio informa que los enlaces share gratuitos expiran después de 72 horas; el cuaderno puede recrear el túnel al reiniciarse.
