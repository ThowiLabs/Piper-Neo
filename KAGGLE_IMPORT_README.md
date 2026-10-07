# Piper-Neo Studio · Kaggle desde el repositorio oficial

## Único origen de código

Repositorio: **https://github.com/ThowiLabs/Piper-Neo**

Rama con Studio, HF Resume y respaldos incrementales:
`feature/resilient-finetune-studio`

Notebook oficial de la rama:
`notebooks/KAGGLE_IMPORT_PIPER_NEO_STUDIO.ipynb`

El notebook **NO pregunta** repositorio, no solicita ZIP y no permite elegir
entre GitHub/ZIP. Clona directamente la rama de Studio del proyecto original.

## Ejecutar en Kaggle

1. Importa `notebooks/KAGGLE_IMPORT_PIPER_NEO_STUDIO.ipynb` en un Kaggle nuevo.
2. En Settings activa GPU NVIDIA (p. ej., Tesla T4) e Internet.
3. Ejecuta las celdas en orden. Se clonará
   `https://github.com/ThowiLabs/Piper-Neo.git` en
   `/kaggle/working/Piper-Neo`, rama `feature/resilient-finetune-studio`.
4. El script de instalación fija Python 3.10 y dependencias del fine-tune.
   **El paquete portable precompilado todavía no está terminado**.
5. Se verifican GPU/CUDA y las pruebas de HF backup/resume/inferencia.
6. Opcional: configura Kaggle Secrets `HF_TOKEN`, `PIPER_STUDIO_AUTH_USER`
   y `PIPER_STUDIO_AUTH_PASSWORD`. Nunca pegues el token en código público.
7. Abre Gradio desde la última celda de arranque y usa las cinco pestañas:
   Dataset, Preprocess, Entrenamiento, Estado y checkpoints e Inferencia.

## Cuaderno antiguo retirado

Se eliminó de la rama de Studio `notebooks/kaggle_finetune_gradio.ipynb`.
Ya no forma parte del árbol de archivos de esa rama.

**Importante sobre Git:** un commit de eliminación retira el archivo del árbol
actual después de hacer push. No borra por la fuerza los commits históricos
que lo añadieron o modificaron. Reescribir commits remotos publicados podría
romper el historial de otros usuarios; no es necesario para retirar el cuaderno.

Para publicar la corrección a la rama existente:

```bash
git switch feature/resilient-finetune-studio
git push origin feature/resilient-finetune-studio
```

No se requiere `git push --force`. Si se desea llevar el Studio a `main`,
revísalo primero en un Pull Request hacia la rama principal.

## ZIP de respaldo Git

También se entrega un ZIP del árbol actual con toda su carpeta `.git` y
commits para respaldar el trabajo. **El notebook no lo utiliza**: siempre
clona el repositorio oficial.

El ZIP no incluye entorno virtual `.venv`, pesos `.ckpt`, datasets, audio
entrenado, secretos ni cachés. Estos recursos se descargan o crean en Kaggle.
El estado de entrenamientos anteriores no se recupera únicamente clonando Git;
para continuar desde HF también es necesario preparar el dataset original.

## Verificación y seguridad

- Los respaldos HF comprueban archivos existentes en el repo remoto antes de
  subir otros nuevos y conservan versiones anteriores sin borrarlas.
- Para reanudar, se utiliza el último checkpoint válido por `global_step`
  y `--resume_from_checkpoint` con optimizadores/época.
- Los checkpoints Lightning usan pickle y **solo deben recuperarse de fuentes
  confiables**.
- Un enlace Gradio público sin credenciales no es privado; recomendamos activar
  autenticación antes de administrar datos o HF desde el navegador.

## Diagnosticar errores al preparar datasets en otra instancia Kaggle

En **1. Dataset**, el botón **Preparar y validar dataset** ahora devuelve dentro
del cuadro de Validación la etapa, el tipo de excepción y el motivo, incluso
para errores inesperados de descarga ZIP, lectura CSV o acceso al disco.
El cuadro **Diagnóstico técnico / traceback** incluye la pila completa y
permite descargar el registro desde
`/kaggle/working/piper_finetune/diagnostics/dataset_<proyecto>.log`.

Si el archivo no llega a Gradio (por error de transferencia del CSV o ZIP),
el código de validación ni siquiera se ejecuta. En ese caso presiona
**Ver diagnóstico del servidor y dataset**: consulta también el archivo
`/kaggle/working/Piper-Neo/studio_kaggle.log` si lanzaste Gradio
mediante este notebook. El servidor conserva `show_error=True`.
Si no aparece ni en ese registro, revisa la pestaña Network/Console del
navegador y comprueba la carga del archivo.

El notebook configura `MPLBACKEND=Agg` en los procesos aislados para evitar
el error de `matplotlib_inline.backend_inline` en Kaggle.

Estos cambios **solo se aplican a instancias que ejecuten el nuevo commit**:
hay que hacer push de la rama y lanzar la versión actualizada; recargar
el navegador de un servidor Gradio antiguo no actualiza su código Python.

## Hugging Face privado: respaldo y reanudación

Repositorio predeterminado: `HirCoir/piper-checkpoint-es-mx-capybara` (tipo **model**, privado).

**Token:** El mismo campo de Gradio sirve para respaldo y recuperación. Leer y reanudar necesita permiso **Read** sobre el repositorio privado; subir checkpoints necesita **Write**. Usa `HF_TOKEN` mediante Kaggle Secrets o escribe un token en **HF token**. El token jamás se guarda en preferencias o archivos de trabajo. El botón **Comprobar acceso privado HF (solo lectura)** confirma identidad y acceso sin modificar el repositorio; no puede certificar permiso de escritura hasta intentar una subida real.

**Resume:** En **3. Entrenamiento**, prepara y preprocesa primero el mismo dataset original (HF no contiene `dataset.jsonl`). Selecciona **Resume de corrida** y **Hugging Face: último válido**. El repositorio de origen puede dejarse vacío: se usa automáticamente `HirCoir/piper-checkpoint-es-mx-capybara`. Pulsa **Consultar checkpoints HF**, elige el más reciente válido y asegúrate de que `max_epochs` sea mayor que la época recuperada. La restauración conserva `global_step` y los estados de optimizadores.

**Backup:** El respaldo compara el contenido remoto antes de subir y nunca elimina ni sobrescribe checkpoints antiguos. No intenta crear un repositorio privado que ya existe. Si falla la subida por API, verifica primero que el archivo no haya aparecido; si sigue faltando, intenta la CLI mediante el token en una variable de entorno (no en el comando). Los checkpoints se comprueban por tamaño y hash.

**Cambio de token:** Pulsa de nuevo **Activar respaldo HF independiente** con el token vigente en **el Gradio dueño del watcher**. Si coincide el proyecto/repositorio, actualiza las credenciales en memoria sin detener el fine-tune. Otros servidores Gradio tienen watchers independientes y no se actualizan automáticamente. El secreto caduca si así fue configurado; para respaldos prolongados usa un token con permisos mínimos y vigencia suficiente.

**Actualización del código:** Después de publicar los commits en GitHub, el servidor Gradio antiguo no cargará los cambios por refrescar el navegador. Inicia un nuevo proceso Gradio cuando sea seguro; evita reiniciar el kernel si el entrenamiento activo debe seguir funcionando.
