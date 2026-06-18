#include "help_text.hpp"

#include <iostream>

namespace piper_app {

void printUsage(char *argv[]) {
  std::cerr << "\nusage: " << argv[0] << " [options]\n\n";
  std::cerr << "options:\n";
  std::cerr << "   -h        --help              show this message and exit\n";
  std::cerr << "   -m  FILE  --model       FILE  path to onnx/.neo model file\n";
  std::cerr << "   -c  FILE  --config      FILE  path to model config file (default: model path + .json)\n";
  std::cerr << "   -f  FILE  --output_file FILE  path to output WAV file ('-' for stdout)\n";
  std::cerr << "   -d  DIR   --output_dir  DIR   path to output directory (default: cwd)\n";
  std::cerr << "   --output_raw                  output raw audio to stdout as it becomes available\n";
  std::cerr << "   -s  NUM   --speaker     NUM   id of speaker (default: 0)\n";
  std::cerr << "   --noise_scale           NUM   generator noise (default: 0.667)\n";
  std::cerr << "   --length_scale          NUM   phoneme length (default: 1.0)\n";
  std::cerr << "   --noise_w               NUM   phoneme width noise (default: 0.8)\n";
  std::cerr << "   --sentence_silence      NUM   seconds of silence after each sentence or line break (default: 0.35)\n";
  std::cerr << "   --phoneme_silence       TXT NUM seconds of silence after one codepoint phoneme\n";
  std::cerr << "   --espeak_data           DIR   path to espeak-ng data directory\n";
  std::cerr << "   --tashkeel_model        FILE  path to libtashkeel onnx model (arabic)\n";
  std::cerr << "   --text                  TEXT  synthesize a direct text payload\n";
  std::cerr << "   --input_file            FILE  read text from a UTF-8 text file\n";
  std::cerr << "   --json-input                  stdin input is lines of JSON instead of plain text\n";
  std::cerr << "   --server                      start local HTTP API server\n";
  std::cerr << "   --serve                       alias for --server\n";
  std::cerr << "   --host                  IP    server host (default: 127.0.0.1)\n";
  std::cerr << "   --port                  NUM   server port (default: 8080)\n";
  std::cerr << "   --models                DIR   models directory for server mode (default: models/)\n";
  std::cerr << "   --api-token             TOKEN protect API server with bearer token\n";
  std::cerr << "                                env/.env: PIPER_API_TOKEN\n";
  std::cerr << "   --cpu-profile           NAME  auto resource profile: auto, eco, balanced, fast, max (server default: auto)\n";
  std::cerr << "   --max-concurrent-jobs   NUM   max accepted simultaneous API jobs (default: auto)\n";
  std::cerr << "   --chunk-workers         NUM   fair chunk worker count (default: auto)\n";
  std::cerr << "   --max-model-replicas    NUM   max ONNX replicas per model (default: auto)\n";
  std::cerr << "   --queue-size            NUM   max waiting/active API jobs (default: auto)\n";
  std::cerr << "   --queue-timeout-seconds NUM   seconds a job may wait in queue (default: 60)\n";
  std::cerr << "   --max-temp-bytes        NUM   max temporary RAW audio bytes (default: auto from memory/cgroup)\n";
  std::cerr << "   --output-retention-seconds NUM seconds generated API WAV files stay available (default: 3600)\n";
  std::cerr << "   --models-refresh-seconds NUM seconds to cache /models metadata (default: 30)\n";
  std::cerr << "   --export-neo            FILE export --model/--config into a Piper Neo .neo package\n";
  std::cerr << "   --neo-image             FILE optional cover image for --export-neo\n";
  std::cerr << "   --neo-compression-level NUM  zstd compression level for .neo export (default: 10)\n";
  std::cerr << "   --max-input-bytes       NUM   max API/direct input bytes (default: 10485760)\n";
  std::cerr << "   --use-cuda                    use CUDA execution provider\n";
  std::cerr << "   --cpu-threads           NUM   limit ONNX Runtime CPU threads\n";
  std::cerr << "   --max-text-chunk-bytes  NUM   preferred smart chunk size before synthesis (default: 4096)\n";
  std::cerr << "   --version                     print Piper Neo version and exit\n";
  std::cerr << "   --debug                       print DEBUG messages to the console\n";
  std::cerr << "   -q       --quiet              disable logging\n\n";
}

} // namespace piper_app
