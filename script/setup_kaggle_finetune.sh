#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
PY_DIR="$ROOT_DIR/src/python"

cd "$PY_DIR"

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
