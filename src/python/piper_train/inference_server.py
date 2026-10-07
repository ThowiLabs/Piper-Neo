"""Isolated read-only public inference Gradio, runs alongside an active trainer.

Does not expose dataset deletion, training start/stop, or Hugging Face upload.
Never binds to the same port as the training dashboard.
"""
from __future__ import annotations

import argparse
import os
import time
from pathlib import Path

import gradio as gr
from .gradio_finetune import _project_dir, KAGGLE_ROOT
from .inference_lab import find_checkpoints, synthesize, export_onnx


def refresh(project_name):
    project = _project_dir(project_name)
    checkpoints = [
        str(path)
        for path in find_checkpoints(project)
        if time.time() - path.stat().st_mtime > 20
    ]
    return gr.update(
        choices=checkpoints, value=checkpoints[0] if checkpoints else None
    )


def run_tts(name, checkpoint, uploaded, config, text, language, speed, noise, noise_w):
    try:
        return synthesize(
            _project_dir(name), checkpoint or "", uploaded, config,
            text, language, float(speed), float(noise), float(noise_w)
        )
    except Exception as error:
        raise gr.Error(str(error)) from error


def run_export(name, checkpoint, uploaded, config):
    try:
        return export_onnx(_project_dir(name), checkpoint or "", uploaded, config)
    except Exception as error:
        raise gr.Error(str(error)) from error


def create_ui():
    with gr.Blocks(title="Piper-Neo Studio — inferencia aislada") as demo:
        gr.Markdown(
            "# Piper-Neo Studio · Laboratorio de inferencia\n"
            "**Servidor de prueba independiente.** No interrumpe ni controla el "
            "entrenamiento activo: la síntesis se realiza en CPU.\n"
            "Checkpoint de confianza + config.json → audio WAV u ONNX validado."
        )
        project_name = gr.Textbox(value="capibara", label="Proyecto existente")
        refresh_btn = gr.Button("Actualizar checkpoints disponibles")
        checkpoint = gr.Dropdown(
            label="Checkpoint entrenado (elegir después de actualizar)",
            choices=[], allow_custom_value=False,
        )
        refresh_btn.click(refresh, inputs=project_name, outputs=checkpoint)
        gr.Markdown(
            "Puedes seleccionar el último checkpoint estable o subir uno propio. "
            "**No subas checkpoints de origen desconocido**: el formato CKPT contiene pickle."
        )
        with gr.Row():
            ckpt_upload = gr.File(
                label="Subir checkpoint .ckpt (opcional)", file_types=[".ckpt"],
                type="filepath",
            )
            config_upload = gr.File(
                label="Subir config.json (opcional)", file_types=[".json"],
                type="filepath",
            )
        text = gr.Textbox(
            label="Texto a pronunciar",
            lines=4,
            value="Hola, esta es una prueba de voz en español de México.",
        )
        language = gr.Textbox(label="Idioma eSpeak", value="es-419")
        with gr.Row():
            length_scale = gr.Slider(0.5, 2, value=1, step=.05, label="Duración (menos = más rápido)")
            noise_scale = gr.Slider(.1, 1, value=.667, step=.01, label="Variación de voz")
            noise_w = gr.Slider(.1, 1, value=.8, step=.01, label="Variación de duración")
        synth_btn = gr.Button("Generar audio", variant="primary")
        audio = gr.Audio(label="Escuchar voz", interactive=False, type="filepath")
        wav = gr.File(label="Descargar WAV", interactive=False)
        report = gr.Textbox(label="Resultado", lines=5)
        synth_btn.click(
            run_tts,
            inputs=[
                project_name, checkpoint, ckpt_upload, config_upload,
                text, language, length_scale, noise_scale, noise_w,
            ],
            outputs=[audio, wav, report],
        )
        gr.Markdown("### Exportar modelo a ONNX")
        export_btn = gr.Button("Exportar ONNX y probar síntesis")
        files = gr.File(label="ONNX + .onnx.json", file_count="multiple", interactive=False)
        export_info = gr.Textbox(label="Validación ONNX", lines=4)
        export_btn.click(
            run_export, inputs=[project_name, checkpoint, ckpt_upload, config_upload],
            outputs=[files, export_info],
        )
    return demo


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=7862)
    parser.add_argument("--share", action="store_true")
    args = parser.parse_args()
    os.makedirs(KAGGLE_ROOT, exist_ok=True)
    create_ui().queue(default_concurrency_limit=1).launch(
        server_name="0.0.0.0", server_port=args.port,
        share=args.share, show_error=True,
        allowed_paths=[str(KAGGLE_ROOT)],
        max_file_size="2gb",
    )


if __name__ == "__main__":
    main()
