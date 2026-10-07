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

El notebook quedó fijado al commit validado `6fc3dc96ebe45adcd956a9a07d96bcd339bc5e0e` para que el bootstrap no dependa de cambios futuros en `main`.


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

El notebook está fijado al commit funcional 6fc3dc96ebe45adcd956a9a07d96bcd339bc5e0e, por lo que su bootstrap no depende de cambios futuros en main.

Validación de servidor:
- servidor local: OK / HTTP 200
- túnel público Gradio: OK
- Gradio informa que los enlaces share gratuitos expiran después de 72 horas; el cuaderno puede recrear el túnel al reiniciarse.

- `6fc3dc96ebe45adcd956a9a07d96bcd339bc5e0e` — Ensure Kaggle NVIDIA libraries for fine-tune jobs

- `6fc3dc96ebe45adcd956a9a07d96bcd339bc5e0e` — Harden Kaggle CUDA discovery for Gradio jobs


## Prueba GPU real — 2026-10-07

- Se identificó una diferencia entre el kernel de Kaggle y el proceso MCP:
  - los dispositivos `/dev/nvidia0` y `/dev/nvidia1` estaban visibles;
  - el driver del host era NVIDIA 580.178.04;
  - `libcuda.so` estaba en `/usr/local/nvidia/lib64`;
  - esa ruta no estaba incluida en el loader del proceso MCP.
- Corrección aplicada:
  - `LD_LIBRARY_PATH=/usr/local/nvidia/lib64` para procesos de entrenamiento;
  - preload explícito de `/usr/local/nvidia/lib64/libcuda.so.1` en Gradio antes de importar torch.
- Después de la corrección, PyTorch detecta:
  - CUDA disponible: True
  - GPU count: 2
  - GPU 0: Tesla T4
  - GPU 1: Tesla T4
- Fine-tune real ejecutado sobre Capibara con:
  - accelerator: gpu
  - CUDA_VISIBLE_DEVICES: 0
  - batch size: 8
  - max_phoneme_ids: 400
  - checkpoint base: davefx-medium
  - fine-tune mediante --init-from-checkpoint
- Duración antes de terminación manual: ~74 s de ejecución antes de SIGTERM (~81 s incluyendo apagado).
- Se alcanzó `epoch=0-step=84.ckpt`.
- Monitoreo NVML durante entrenamiento:
  - utilización observada: hasta 99%
  - muestra sostenida: 88%, 99%, 53%, 58%, 84%
  - pico de memoria observado del proceso: ~10.7 GiB
  - GPU 1 permaneció libre.
- El trainer reportó explícitamente:
  - `GPU available: True (cuda), used: True`
  - `LOCAL_RANK: 0 - CUDA_VISIBLE_DEVICES: [0]`
  - `Fine-tune initialization loaded 784/784 compatible tensors`
- Gradio fue validado con CUDA visible y túnel público.


## Protección de entrenamiento activo durante pruebas de inferencia — 2026-10-07

- Gradio de entrenamiento existente: puerto 7861, PID 1713 (NO detenido ni reiniciado).
- Fine-tune GPU activo: PID 1734, dataset capibara/training, batch 12 y checkpoint cada 15 minutos.
- Checkpoint confirmado mientras el proceso corría: epoch=2-step=1382.ckpt.
- Nuevo servidor de inferencia SOLO LECTURA/PRUEBAS: puerto 7862, PID 1930, https://04ef9039ff44e73cc7.gradio.live (temporal).
- Nuevo módulo de arranque aislado: src/python/piper_train/inference_server.py; genera WAV con CPU, exporta ONNX y no ofrece comandos de entrenamiento ni eliminación de datasets.
- Enlace público verificado mediante HTTP 200.
- Construcción del tarball portable detenida sin afectar el entrenamiento: el archivo parcial fue eliminado para reservar disco (5.1 GB libres, 75% usado).
- Próximo empaquetado: hacerlo cuando el entrenamiento termine o fuera del disco de trabajo compartido, para evitar interferencia y falta de espacio.


## Gradio integrado + respaldos HF en raíz + restauración — 2026-10-07

- Rama de desarrollo: `feature/resilient-finetune-studio`; `main` NO modificado.
- Cinco pestañas en el entrenamiento, con inferencia integrada como pestaña `5. Inferencia y exportación ONNX`.
- Respaldos a HF ahora delegados a `hf_backup.sync_verified_checkpoints`:
  - ruta HF: **raíz del repo**, `config.json` + `epoch=N-step=N.ckpt`.
  - conserva nombres originales, NO usa carpetas, NO elimina ningún archivo del repo HF.
  - valida checkpoint completo antes de subir: estabilidad/snapshot, torch.load, epoch/step, 2 optimizadores, 784 tensores en ejemplo real, compatibilidad config, SHA-256, verificación remota de tamaño.
  - reintenta fallos; manifest local evita subir de nuevo el mismo contenido.
  - limitación importante: validación estructural no garantiza calidad perceptual del audio; inferencia ofrece comprobación de audio.
  - pruebas simuladas HF: rechaza archivos inválidos sin subir nada; repo en raíz; idempotencia; confirmación remota.
  - **No hubo subida real a HF**, ya que falta token del usuario.
- Un monitor HF independiente (`hf_watcher.py`) puede seguir subiendo checkpoints generados por otra instancia mientras ésta entrena; no necesita reiniciar ni controlar el trainer. Token solo en RAM/variable de entorno, no en JSON de preferencias.
- Preferencias no secretas se guardan atómicamente por proyecto en `.studio_preferences.json` y último proyecto en `.studio_last_project.json`; se recargan en `demo.load` al refrescar. Se recuperan estado/log/checkpoints y selección de inferencia; no se restauran secretos ni archivos de subida temporales por seguridad.
- Guardas de proceso comparan `/proc/*/cmdline` con el `--dataset-dir` antes de iniciar, borrar o preprocesar un proyecto existente. Se detectó fine-tune activo PID 1734 en `/kaggle/working/piper_finetune/capibara/training`.
- Nueva instancia segura Gradio: puerto 7863, autenticación habilitada, enlace temporal `https://e4c75bf834928de3e4.gradio.live` (no guardar credenciales en el repo).
- Se conservaron sin reiniciar los servidores 7860 y 7861, y el proceso GPU de entrenamiento. Checkpoint observado durante última verificación: `epoch=5-step=2860.ckpt`.
- Se cerró SOLO el antiguo servidor de inferencia 7862 tras iniciar la nueva web de entrenamiento.
- 7 tests `unittest` completados y `git diff --check` sin errores.
- Construcción del paquete portátil aplazada mientras se entrena para preservar espacio en disco.


## Nueva versión: HF Resume + controles inteligentes — 2026-10-07

- Rama: `feature/resilient-finetune-studio` sin modificaciones a `main`.
- Opciones de origen del resume: último checkpoint local / subida o URL / Hugging Face.
- HF Resume lee solamente archivos raíz de repos de tipo `model`, requiere `config.json`.
- Lista checkpoints por `global_step` (más reciente primero), no por época ni orden alfabético.
- Puede usar selección `Automático: último checkpoint válido`, probando checkpoints anteriores si el nuevo está roto, o una versión explícita sin fallback.
- Permite nombre corto de repo con token para resolver namespace o `usuario/nombre`.
- Descarga metadata + checkpoint con `hf_hub_download`, comprueba metadatos HF, SHA256 cuando está publicado, compatibilidad de `config.json` con dataset local, estructura Lightning y optimizadores.
- Jamás reemplaza `training/config.json` ni el dataset local durante recuperación HF.
- Genera manifiesto sin token en `resume_hf/<repo>/.last_resume.json`.
- Pasa el checkpoint al trainer mediante `--resume_from_checkpoint` para conservar optimizadores y época, no mediante init-from-checkpoint.
- Gradio recopila preferencias de fuente HF y selección de versión; tokens no persisten en el archivo de preferencias.
- 17 pruebas unitarias/regresión realizadas correctamente, incluida preparación del comando de reanudación.
- Se probó flujo completo de prevalidación de un `epoch=5-step=2860.ckpt` real mediante mock remoto en el mismo volumen (SHA256 y `global_step=2860`).
- Nueva web pública **sin auth al no configurar user/pass** en puerto 7864:
  `https://5e187940e49fe442e7.gradio.live`, HTTP 200, cinco pestañas, menú HF resume visible.
- Los puertos 7860, 7861 y 7863 se mantuvieron; no se interrumpió ninguna instancia.
- No hubo prueba privada HF real por falta de token, ni se hizo push.
- El empaquetado portable sigue pendiente por gestión de espacio del notebook.
