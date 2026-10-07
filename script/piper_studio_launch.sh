#!/usr/bin/env bash
# Self-contained Linux x86_64 Piper Neo Studio launcher.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [[ "$(uname -m)" != "x86_64" ]]; then
  echo "Piper Neo portable requires x86_64 Linux." >&2
  exit 2
fi
export PYTHONNOUSERSITE=1
export PYTHONPATH="$ROOT/app:$ROOT/site-packages"
export LD_LIBRARY_PATH="/usr/local/nvidia/lib64:${LD_LIBRARY_PATH:-}"
export PIPER_FINETUNE_ROOT="${PIPER_FINETUNE_ROOT:-/kaggle/working/piper_finetune}"
PY="$ROOT/python/bin/python3.10"
if [[ ! -x "$PY" ]]; then
  echo "Bundled Python is missing. Re-download/verify the runtime." >&2
  exit 3
fi
echo "Piper Neo Studio portable runtime: $ROOT"
"$PY" -c 'import torch; print("PyTorch", torch.__version__); print("CUDA", torch.cuda.is_available(), "GPUs", torch.cuda.device_count()); print("GPU names", [torch.cuda.get_device_name(i) for i in range(torch.cuda.device_count())])'
exec "$PY" -u -m piper_train.gradio_finetune "$@"
