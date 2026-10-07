# Piper-Neo Studio · Transferir Git + probar en un nuevo notebook

## Contenido del paquete ZIP

`Piper-Neo-Studio-GIT.zip` incluye:
- Proyecto con código fuente y todos los archivos actualmente versionados.
- Carpeta oculta **`.git/` con historial y ramas**:
  - `feature/resilient-finetune-studio` (rama que contiene el Studio completo).
  - `main` (punto de partida).
- Cuaderno `notebooks/KAGGLE_IMPORT_PIPER_NEO_STUDIO.ipynb` para un Kaggle nuevo.
- Validación de checkpoints, respaldo incremental orientado al repo HF (sin borrar versiones),
  recuperación desde HF, inferencia y exportación ONNX.
- Dependencias fijadas y script `script/setup_kaggle_finetune.sh`.

**No incluye** `.venv` (más de 4 GB), ningún dataset, CSV con transcripciones,
checkpoint/ONNX/WAV, token HF, cachés ni archivos de salida de entrenamiento.
No se incluyen ejecutables precompilados portables: los scripts experimentales de empaquetado
no constituyen una build terminada ni validada en hardware diferente.

## Subir un repositorio nuevo a GitHub CONSERVANDO commits

**IMPORTANTE:** subir el ZIP directamente con «Upload files» de GitHub crea un
archivo .zip en el repositorio, **NO** reconstruye la historia de commits.
Para conservar la historia hay que utilizar Git en una terminal.

1. Crea un repositorio **VACÍO** en GitHub sin README, .gitignore ni licencia.
2. Extrae el ZIP en tu computadora (verifica que `Piper-Neo/.git/` exista; en
   Windows hay que permitir archivos ocultos).
3. Abre una terminal en el directorio extraído y ejecuta:

```bash
cd Piper-Neo
git branch -avv
git log -3 --oneline
git remote set-url origin https://github.com/TU_USUARIO/Piper-Neo.git
git push -u origin feature/resilient-finetune-studio
git push origin main
```

Si prefieres que la versión nueva sea el `main` del nuevo repositorio, se puede
configurar su rama por defecto en GitHub, o integrar la rama feature después de
revisar los cambios. El notebook de esta entrega clona explícitamente la rama
`feature/resilient-finetune-studio`.

**Nota de permisos:** el Git author de los commits existentes es `YahirHub`; su
identidad no da credenciales para pushear al GitHub de otra persona. Para hacer
push tendrás que autenticarte en tu propia cuenta.

## Probar en Kaggle

1. Kaggle → `New Notebook`, importar
   `notebooks/KAGGLE_IMPORT_PIPER_NEO_STUDIO.ipynb`.
2. Settings → activar **GPU** (Tesla T4 u otra compatible) e **Internet ON**.
3. Ejecuta la primera celda; elige `SOURCE = "GitHub"` y pon la URL de tu
   repositorio, o `SOURCE = "ZIP"` y la ruta del ZIP que adjuntaste vía Add Input.
4. Ejecuta la importación y confirma que `git log` incluye `f7b4358`
   y los commits posteriores.
5. Ejecuta setup (descarga Python 3.10 + dependencias). Necesita Internet y espacio
   libre suficiente. La build portable aún está pendiente; por ahora el setup
   instala dependencias fijadas y compila los módulos necesarios.
6. Ejecuta la comprobación CUDA y las pruebas. Comprueba las GPU visibles.
7. Opcional: en Add-ons/Secrets, configura `HF_TOKEN` y/o
   `PIPER_STUDIO_AUTH_USER` + `PIPER_STUDIO_AUTH_PASSWORD`.
8. Lanza Gradio; la celda mostrará una URL `*.gradio.live`.
9. En la pestaña 1 prepara WAV/ZIP y CSV, en la 2 preprocesa en `es-419`,
   en la 3 entrena o reanuda desde HF, en la 4 respalda checkpoints nuevos y en
   la 5 prueba la inferencia.

## Seguridad y persistencia

- No publiques tokens de Hugging Face en una URL pública de Gradio sin contraseña.
  Prefiere Secrets de Kaggle y permisos mínimos.
- El backup es **incremental y append-only**: compara contra archivos remotos,
  sin borrar ni sobrescribir checkpoints anteriores.
- Se necesita el dataset preprocesado original para usar Resume desde un checkpoint
  de HF. Los checkpoints no contienen todo el dataset.
- Si Kaggle se reinicia, los archivos en `/kaggle/working` podrían perderse.
  Haz respaldo de los checkpoints en HF. El enlace temporal de Gradio no es hosting
  permanente.
- La rama del paquete no se subió a GitHub por parte del asistente; está
  incluida íntegramente en este ZIP para que puedas crear tu propio remoto.
