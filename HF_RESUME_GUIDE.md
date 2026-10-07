# Piper-Neo Studio — recuperar un fine-tune desde Hugging Face

Esta función reanuda una corrida de entrenamiento real (pesos, `epoch`, `global_step`,
optimizadores y schedulers), no vuelve a inicializar un modelo desde cero.

## Requisitos

- Haber preparado/preprocesado en Kaggle el **mismo dataset** para el que se guardó la corrida.
  Deben existir `training/dataset.jsonl` y `training/config.json`.
- En el repositorio **modelo** de Hugging Face deben existir, **en la raíz**:
  - `config.json`
  - `epoch=0-step=....ckpt`, `epoch=1-step=....ckpt`, etc.
- El repositorio debe ser confiable. Los checkpoints Lightning utilizan pickle,
  cuya deserialización puede ejecutar código: evita orígenes desconocidos.
- Para repositorios privados debes ingresar un token con permiso de lectura
  o establecer `HF_TOKEN` en las variables del entorno del servidor.
- Si ingresas solo `nombre-modelo` como repo, el usuario se infiere del token.
  También puedes usar `usuario/nombre-modelo`.
- Hay que disponer de aproximadamente 1 GB libre por checkpoint descargado,
  y espacio para futuros checkpoints locales.

## En la web

1. Abre `3. Entrenamiento`.
2. En `Modo` elige **Resume de corrida**.
3. En `Origen para reanudar` elige **Hugging Face: último válido**.
4. Escribe el repositorio HF de origen en `Repositorio HF de origen` (si lo dejas
   vacío, se utilizará el repositorio configurado para backup).
5. Introduce el token HF si es privado (o configura `HF_TOKEN`).
6. Usa **Consultar checkpoints HF** para verlos ordenados por **global_step**
   (último paso entrenado). Puedes elegir una versión específica o dejar
   **Automático: último checkpoint válido**.
7. Asegúrate de que `max_epochs` supere la época del checkpoint elegido.
8. Haz clic en **Iniciar entrenamiento**. El sistema:
   - Comprueba que no exista otro entrenamiento activo en ese proyecto.
   - Consulta la raíz del repo, descargando `config.json` y el checkpoint.
   - Compara configuración de idioma/phonemes/símbolos/speakers/sample rate
     contra el dataset local, sin sobrescribirlo.
   - Comprueba tamaño y SHA-256 remoto (si HF publica el digest).
   - Valida `state_dict`, época, paso, optimizadores y SHA-256 local.
   - Si está en modo automático, intenta uno anterior cuando el más reciente
     no se valida, y registra los fallos.
   - Inicia el entrenador con **`--resume_from_checkpoint`**.
   - Guarda un manifiesto sin token en `resume_hf/REPO/.last_resume.json`.
9. Puedes activar el respaldo HF automático de la nueva corrida y/o subir los
   checkpoints manualmente desde `4. Estado y checkpoints`.

## Respaldo a HF

Los checkpoints verificados se guardan con nombre original directamente en la
raíz del repo, junto a `config.json`. El sistema **no elimina checkpoints remotos**
y rechaza sobrescrituras de nombre que no puedan probarse idénticas. Si el repo
no existe, se crea con `HfApi.create_repo(exist_ok=True)` cuando se encuentra
un checkpoint válido para publicar.

## Persistencia y advertencias

- Recargar el navegador restaura las preferencias guardadas, excepto tokens y
  rutas de archivos temporales subidos.
- El entrenamiento continúa independientemente del navegador.
- El respaldo HF automático reside en el proceso Gradio y sobrevive a recargas
  de página, pero **no** a que muera el proceso/entorno; para reiniciar hay que
  relanzar Gradio y reactivar la tarea de respaldo con el token o `HF_TOKEN`.
- No se garantiza un servicio público permanente en Kaggle: las URLs temporales
  expiran y el entrenamiento se detiene si el runtime muere.
- Una validación estructural correcta **no garantiza calidad de voz**. Utiliza
  `5. Inferencia y exportación ONNX` para escuchar el resultado.
- El enlace público de Gradio sin contraseña está abierto a cualquiera que lo
  conozca; evita poner tokens en campos de un servidor público sin autenticación.
  Para usos prolongados, utiliza credenciales o secretos del entorno.

## Cobertura de pruebas

- Tests de versiones por global_step, selección explícita, fallback desde un
  checkpoint inválido, configuración de espeak incompatible, ausencia de dataset,
  y construcción correcta de la orden `--resume_from_checkpoint`.
- Smoke de validación con el checkpoint real
  `epoch=5-step=2860.ckpt` (`global_step=2860`, epoch 5) usando un mock de HF
  (sin hacer una petición externa ni iniciar un entrenamiento nuevo).
- Falta aún una prueba con un token y un repositorio privado real.
