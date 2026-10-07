import argparse
import json
import logging
from datetime import timedelta
from pathlib import Path

import torch
from pytorch_lightning import Trainer
from pytorch_lightning.callbacks import ModelCheckpoint
from packaging.version import Version
import pytorch_lightning as pl

from .vits.lightning import VitsModel

_LOGGER = logging.getLogger(__package__)


def main():
    logging.basicConfig(level=logging.DEBUG)

    # Piper Neo's VITS trainer intentionally uses the Lightning 1.x API:
    # training_step(..., optimizer_idx) and Trainer argparse integration.
    # Newer Lightning releases removed that API and fail later with much less
    # useful errors (for example, Trainer.add_argparse_args missing).
    if Version(pl.__version__) >= Version("2.0.0"):
        raise RuntimeError(
            "Piper Neo training requires pytorch-lightning 1.9.x. "
            f"Detected {pl.__version__}. Install the pinned training requirements "
            "from src/python/requirements.txt."
        )

    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--dataset-dir", required=True, help="Path to pre-processed dataset directory"
    )
    parser.add_argument(
        "--checkpoint-epochs",
        type=int,
        help="Save checkpoint every N epochs (default: 1)",
    )
    parser.add_argument(
        "--checkpoint-minutes",
        type=float,
        help=(
            "Save a checkpoint at least every N minutes of training time. "
            "When set, this takes precedence over --checkpoint-epochs."
        ),
    )
    parser.add_argument(
        "--quality",
        default="medium",
        choices=("x-low", "medium", "high"),
        help="Quality/size of model (default: medium)",
    )
    parser.add_argument(
        "--resume_from_single_speaker_checkpoint",
        help="For multi-speaker models only. Converts a single-speaker checkpoint to multi-speaker and resumes training",
    )
    parser.add_argument(
        "--init-from-checkpoint",
        help=(
            "Initialize model weights from a checkpoint but start a NEW training run "
            "from epoch 0 with fresh optimizer/scheduler state. Use this for fine-tuning "
            "from a pretrained Piper checkpoint. Use Lightning's --resume_from_checkpoint "
            "only to resume an interrupted run."
        ),
    )
    Trainer.add_argparse_args(parser)
    VitsModel.add_model_specific_args(parser)
    parser.add_argument("--seed", type=int, default=1234)
    args = parser.parse_args()
    _LOGGER.debug(args)

    if args.init_from_checkpoint and getattr(args, "resume_from_checkpoint", None):
        parser.error(
            "--init-from-checkpoint and --resume_from_checkpoint are mutually exclusive. "
            "Use --init-from-checkpoint for fine-tuning from a base model, or "
            "--resume_from_checkpoint to continue an interrupted run."
        )

    args.dataset_dir = Path(args.dataset_dir)
    if not args.default_root_dir:
        args.default_root_dir = args.dataset_dir

    torch.backends.cudnn.benchmark = True
    torch.manual_seed(args.seed)

    config_path = args.dataset_dir / "config.json"
    dataset_path = args.dataset_dir / "dataset.jsonl"

    with open(config_path, "r", encoding="utf-8") as config_file:
        # See preprocess.py for format
        config = json.load(config_file)
        num_symbols = int(config["num_symbols"])
        num_speakers = int(config["num_speakers"])
        sample_rate = int(config["audio"]["sample_rate"])

    trainer = Trainer.from_argparse_args(args)
    if args.checkpoint_minutes is not None and args.checkpoint_minutes > 0:
        trainer.callbacks = [
            ModelCheckpoint(
                train_time_interval=timedelta(minutes=args.checkpoint_minutes),
                save_top_k=1,
            )
        ]
        _LOGGER.debug(
            "Checkpoints will be saved every %s minute(s)",
            args.checkpoint_minutes,
        )
    elif args.checkpoint_epochs is not None:
        trainer.callbacks = [
            ModelCheckpoint(
                every_n_epochs=args.checkpoint_epochs,
                save_top_k=1,
            )
        ]
        _LOGGER.debug(
            "Checkpoints will be saved every %s epoch(s)", args.checkpoint_epochs
        )

    dict_args = vars(args)
    if args.quality == "x-low":
        dict_args["hidden_channels"] = 96
        dict_args["inter_channels"] = 96
        dict_args["filter_channels"] = 384
    elif args.quality == "high":
        dict_args["resblock"] = "1"
        dict_args["resblock_kernel_sizes"] = (3, 7, 11)
        dict_args["resblock_dilation_sizes"] = (
            (1, 3, 5),
            (1, 3, 5),
            (1, 3, 5),
        )
        dict_args["upsample_rates"] = (8, 8, 2, 2)
        dict_args["upsample_initial_channel"] = 512
        dict_args["upsample_kernel_sizes"] = (16, 16, 4, 4)

    model = VitsModel(
        num_symbols=num_symbols,
        num_speakers=num_speakers,
        sample_rate=sample_rate,
        dataset=[dataset_path],
        **dict_args,
    )

    if args.init_from_checkpoint:
        _LOGGER.info(
            "Initializing model weights for fine-tuning from checkpoint: %s",
            args.init_from_checkpoint,
        )
        load_finetune_checkpoint(model, args.init_from_checkpoint)

    if args.resume_from_single_speaker_checkpoint:
        assert (
            num_speakers > 1
        ), "--resume_from_single_speaker_checkpoint is only for multi-speaker models. Use --resume_from_checkpoint for single-speaker models."

        # Load single-speaker checkpoint
        _LOGGER.debug(
            "Resuming from single-speaker checkpoint: %s",
            args.resume_from_single_speaker_checkpoint,
        )
        model_single = VitsModel.load_from_checkpoint(
            args.resume_from_single_speaker_checkpoint,
            dataset=None,
        )
        g_dict = model_single.model_g.state_dict()
        for key in list(g_dict.keys()):
            # Remove keys that can't be copied over due to missing speaker embedding
            if (
                key.startswith("dec.cond")
                or key.startswith("dp.cond")
                or ("enc.cond_layer" in key)
            ):
                g_dict.pop(key, None)

        # Copy over the multi-speaker model, excluding keys related to the
        # speaker embedding (which is missing from the single-speaker model).
        load_state_dict(model.model_g, g_dict)
        load_state_dict(model.model_d, model_single.model_d.state_dict())
        _LOGGER.info(
            "Successfully converted single-speaker checkpoint to multi-speaker"
        )

    trainer.fit(model)


def load_finetune_checkpoint(model, checkpoint_path):
    """Load compatible model weights without restoring trainer state.

    A Lightning checkpoint contains model parameters plus epoch/global step,
    optimizer states, scheduler states, callbacks and loop state. Fine-tuning
    from a pretrained Piper voice should reuse only compatible model weights;
    restoring the full checkpoint is a resume operation.
    """

    checkpoint = torch.load(checkpoint_path, map_location="cpu")
    saved_state_dict = checkpoint.get("state_dict", checkpoint)
    if not isinstance(saved_state_dict, dict):
        raise ValueError(f"Invalid checkpoint state_dict: {checkpoint_path}")

    current_state_dict = model.state_dict()
    compatible_state_dict = {}
    skipped_missing = []
    skipped_shape = []

    for key, saved_value in saved_state_dict.items():
        current_value = current_state_dict.get(key)
        if current_value is None:
            skipped_missing.append(key)
            continue

        if tuple(current_value.shape) != tuple(saved_value.shape):
            skipped_shape.append(
                (key, tuple(saved_value.shape), tuple(current_value.shape))
            )
            continue

        compatible_state_dict[key] = saved_value

    if not compatible_state_dict:
        raise ValueError(
            f"No compatible model weights found in checkpoint: {checkpoint_path}"
        )

    missing_after_load, unexpected_after_load = model.load_state_dict(
        compatible_state_dict, strict=False
    )

    _LOGGER.info(
        "Fine-tune initialization loaded %s/%s compatible tensors",
        len(compatible_state_dict),
        len(current_state_dict),
    )

    if skipped_shape:
        _LOGGER.warning(
            "Skipped %s checkpoint tensor(s) because their shapes differ",
            len(skipped_shape),
        )
        for key, saved_shape, current_shape in skipped_shape[:20]:
            _LOGGER.warning(
                "Shape mismatch for %s: checkpoint=%s current=%s",
                key,
                saved_shape,
                current_shape,
            )

    if skipped_missing:
        _LOGGER.debug(
            "Ignored %s checkpoint tensor(s) not present in the current model",
            len(skipped_missing),
        )

    if unexpected_after_load:
        _LOGGER.debug("Unexpected keys after load: %s", unexpected_after_load)

    if missing_after_load:
        _LOGGER.info(
            "Kept %s current tensor(s) freshly initialized",
            len(missing_after_load),
        )

    return {
        "loaded": len(compatible_state_dict),
        "total_current": len(current_state_dict),
        "skipped_shape": skipped_shape,
        "skipped_missing": skipped_missing,
        "checkpoint_epoch": checkpoint.get("epoch"),
        "checkpoint_global_step": checkpoint.get("global_step"),
    }


def load_state_dict(model, saved_state_dict):
    state_dict = model.state_dict()
    new_state_dict = {}

    for k, v in state_dict.items():
        if k in saved_state_dict:
            # Use saved value
            new_state_dict[k] = saved_state_dict[k]
        else:
            # Use initialized value
            _LOGGER.debug("%s is not in the checkpoint", k)
            new_state_dict[k] = v

    model.load_state_dict(new_state_dict)


# -----------------------------------------------------------------------------


if __name__ == "__main__":
    main()
