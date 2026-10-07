# Piper Neo training environment

The training code in `piper_train` uses the Lightning 1.x multi-optimizer API (`optimizer_idx`).
For reproducible training, use Python 3.10 and the pinned dependencies in `requirements.txt`:

- Python 3.10
- PyTorch 1.13.1
- pytorch-lightning 1.9.5
- torchmetrics 0.11.4
- piper-phonemize 1.1.x
- setuptools 65-70 (Lightning 1.9 still imports `pkg_resources`)

Python 3.12 cannot install the legacy `piper-phonemize` wheel, and Lightning 2.x removed
`Trainer.add_argparse_args` and the optimizer-index training API used by this trainer.


## Kaggle / Gradio fine-tuning

From the repository root:

```sh
bash script/setup_kaggle_finetune.sh
src/python/.venv/bin/piper-finetune-web --share
```

The web UI accepts a public Google Drive URL, a direct HTTP(S) URL, or an uploaded ZIP plus metadata. It separates pretrained-weight initialization from full trainer resume, can save checkpoints by elapsed training time, and can mirror checkpoints plus `config.json` to a Hugging Face model repository.

The Gradio stack is pinned in `requirements-finetune-web.txt` because Gradio 4.44.1 is not compatible with the newest FastAPI/Starlette/Pydantic releases.
