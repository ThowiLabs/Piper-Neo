# Piper Neo training environment

The training code in `piper_train` uses the Lightning 1.x multi-optimizer API (`optimizer_idx`).
For reproducible training, use Python 3.10 and the pinned dependencies in `requirements.txt`:

- Python 3.10
- PyTorch 1.13.1
- pytorch-lightning 1.9.5
- torchmetrics 0.11.4
- piper-phonemize 1.1.x

Python 3.12 cannot install the legacy `piper-phonemize` wheel, and Lightning 2.x removed
`Trainer.add_argparse_args` and the optimizer-index training API used by this trainer.
