#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd -P)"
PY_DIR="$ROOT_DIR/src/python"

# The source checkout is disposable, but datasets, checkpoints, saved
# settings and runtime logs must be in a separate directory.
DATA_DIR="${PIPER_FINETUNE_ROOT:-/kaggle/working/piper_finetune}"
mkdir -p "$DATA_DIR"
DATA_DIR="$(cd "$DATA_DIR" && pwd -P)"
case "$DATA_DIR/" in
  "$ROOT_DIR/"*) echo "ERROR: PIPER_FINETUNE_ROOT no puede estar dentro de Piper-Neo." >&2; exit 1 ;;
esac
export PIPER_FINETUNE_ROOT="$DATA_DIR"
export GRADIO_TEMP_DIR="$DATA_DIR/gradio_tmp"
mkdir -p "$GRADIO_TEMP_DIR"

cd "$PY_DIR"

export LD_LIBRARY_PATH="/usr/local/nvidia/lib64:${LD_LIBRARY_PATH:-}"

if ! command -v uv >/dev/null 2>&1; then
  python -m pip install --upgrade uv
fi

uv python install 3.10
uv venv --python 3.10 .venv

uv pip install --python .venv/bin/python "setuptools==70.3.0"
uv pip install --python .venv/bin/python -e . -r requirements-finetune-web.txt

bash build_monotonic_align.sh

.venv/bin/python - <<'PY'
import torch
import pytorch_lightning as pl
from piper_phonemize import phonemize_espeak

print("torch:", torch.__version__)
print("cuda_available:", torch.cuda.is_available())
print("pytorch_lightning:", pl.__version__)
print("es-419:", phonemize_espeak("Hola México", "es-419"))
PY

echo
echo "Entorno listo."
echo "Lanza Gradio con:"
echo "  $PY_DIR/.venv/bin/python -m piper_train.gradio_finetune --share"
