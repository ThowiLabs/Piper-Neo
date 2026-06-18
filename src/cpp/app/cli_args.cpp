#include "cli_args.hpp"
#include "hardware.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

#include <spdlog/spdlog.h>

#include "../neo_model.hpp"
#include "../piper.hpp"
#include "../server.hpp"

namespace piper_app {

using namespace std;

void printUsage(char *argv[]) {
  cerr << endl;
  cerr << "usage: " << argv[0] << " [options]" << endl;
  cerr << endl;
  cerr << "options:" << endl;
  cerr << "   -h        --help              show this message and exit" << endl;
  cerr << "   -m  FILE  --model       FILE  path to onnx model file" << endl;
  cerr << "   -c  FILE  --config      FILE  path to model config file "
          "(default: model path + .json)"
       << endl;
  cerr << "   -f  FILE  --output_file FILE  path to output WAV file ('-' for "
          "stdout)"
       << endl;
  cerr << "   -d  DIR   --output_dir  DIR   path to output directory (default: "
          "cwd)"
       << endl;
  cerr << "   --output_raw                  output raw audio to stdout as it "
          "becomes available"
       << endl;
  cerr << "   -s  NUM   --speaker     NUM   id of speaker (default: 0)" << endl;
  cerr << "   --noise_scale           NUM   generator noise (default: 0.667)"
       << endl;
  cerr << "   --length_scale          NUM   phoneme length (default: 1.0)"
       << endl;
  cerr << "   --noise_w               NUM   phoneme width noise (default: 0.8)"
       << endl;
  cerr << "   --sentence_silence      NUM   seconds of silence after each "
          "sentence or line break (default: 0.35)"
       << endl;
  cerr << "   --espeak_data           DIR   path to espeak-ng data directory"
       << endl;
  cerr << "   --tashkeel_model        FILE  path to libtashkeel onnx model "
          "(arabic)"
       << endl;
  cerr << "   --text                  TEXT  synthesize a direct text payload"
       << endl;
  cerr << "   --input_file            FILE  read text from a UTF-8 text file"
       << endl;
  cerr << "   --json-input                  stdin input is lines of JSON "
          "instead of plain text"
       << endl;
  cerr << "   --server                      start local HTTP API server" << endl;
  cerr << "   --serve                       alias for --server" << endl;
  cerr << "   --host                  IP    server host (default: 127.0.0.1)"
       << endl;
  cerr << "   --port                  NUM   server port (default: 8080)"
       << endl;
  cerr << "   --models                DIR   models directory for server mode "
          "(default: models/)"
       << endl;
  cerr << "   --api-token             TOKEN protect API server with bearer token"
       << endl;
  cerr << "                                env/.env: PIPER_API_TOKEN" << endl;
  cerr << "   --cpu-profile           NAME  auto resource profile: auto, eco, "
          "balanced, fast, max (server default: auto)"
       << endl;
  cerr << "   --max-concurrent-jobs   NUM   max accepted simultaneous API jobs "
          "(default: auto)"
       << endl;
  cerr << "   --chunk-workers         NUM   fair chunk worker count "
          "(default: auto)"
       << endl;
  cerr << "   --max-model-replicas    NUM   max ONNX replicas per model "
          "(default: auto)"
       << endl;
  cerr << "   --queue-size            NUM   max waiting/active API jobs "
          "(default: auto)"
       << endl;
  cerr << "   --queue-timeout-seconds NUM   seconds a job may wait in queue "
          "(default: 60)"
       << endl;
  cerr << "   --max-temp-bytes       NUM   max temporary RAW audio bytes "
          "(default: auto from memory/cgroup)"
       << endl;
  cerr << "   --output-retention-seconds NUM seconds generated API WAV files stay "
          "available (default: 3600)"
       << endl;
  cerr << "   --models-refresh-seconds NUM seconds to cache /models metadata "
          "(default: 30)"
       << endl;
  cerr << "   --export-neo            FILE export --model/--config into a Piper Neo .neo package"
       << endl;
  cerr << "   --neo-image             FILE optional cover image for --export-neo"
       << endl;
  cerr << "   --neo-compression-level NUM  zstd compression level for .neo export "
          "(default: 10)"
       << endl;
  cerr << "   --max-input-bytes       NUM   max API/direct input bytes "
          "(default: 10485760)"
       << endl;
  cerr << "   --use-cuda                    use CUDA execution provider"
       << endl;
  cerr << "   --cpu-threads           NUM   limit ONNX Runtime CPU threads"
       << endl;
  cerr << "   --max-text-chunk-bytes  NUM   preferred smart chunk size before "
          "synthesis (default: 4096)"
       << endl;
  cerr << "   --debug                       print DEBUG messages to the console"
       << endl;
  cerr << "   -q       --quiet              disable logging" << endl;
  cerr << endl;
}

void ensureArg(int argc, char *argv[], int argi) {
  if ((argi + 1) >= argc) {
    printUsage(argv);
    exit(0);
  }
}

// Parse command-line arguments
void parseArgs(int argc, char *argv[], RunConfig &runConfig) {
  optional<filesystem::path> modelConfigPath;

  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];

    if (arg == "-m" || arg == "--model") {
      ensureArg(argc, argv, i);
      runConfig.modelPath = filesystem::path(argv[++i]);
    } else if (arg == "-c" || arg == "--config") {
      ensureArg(argc, argv, i);
      modelConfigPath = filesystem::path(argv[++i]);
    } else if (arg == "-f" || arg == "--output_file" ||
               arg == "--output-file") {
      ensureArg(argc, argv, i);
      std::string filePath = argv[++i];
      if (filePath == "-") {
        runConfig.outputType = OUTPUT_STDOUT;
        runConfig.outputPath = nullopt;
      } else {
        runConfig.outputType = OUTPUT_FILE;
        runConfig.outputPath = filesystem::path(filePath);
      }
    } else if (arg == "-d" || arg == "--output_dir" || arg == "--output-dir") {
      ensureArg(argc, argv, i);
      runConfig.outputType = OUTPUT_DIRECTORY;
      runConfig.outputPath = filesystem::path(argv[++i]);
      runConfig.outputDirExplicit = true;
    } else if (arg == "--output_raw" || arg == "--output-raw") {
      runConfig.outputType = OUTPUT_RAW;
    } else if (arg == "-s" || arg == "--speaker") {
      ensureArg(argc, argv, i);
      runConfig.speakerId = (piper::SpeakerId)stol(argv[++i]);
    } else if (arg == "--noise_scale" || arg == "--noise-scale") {
      ensureArg(argc, argv, i);
      runConfig.noiseScale = stof(argv[++i]);
    } else if (arg == "--length_scale" || arg == "--length-scale") {
      ensureArg(argc, argv, i);
      runConfig.lengthScale = stof(argv[++i]);
    } else if (arg == "--noise_w" || arg == "--noise-w") {
      ensureArg(argc, argv, i);
      runConfig.noiseW = stof(argv[++i]);
    } else if (arg == "--sentence_silence" || arg == "--sentence-silence") {
      ensureArg(argc, argv, i);
      runConfig.sentenceSilenceSeconds = stof(argv[++i]);
    } else if (arg == "--phoneme_silence" || arg == "--phoneme-silence") {
      ensureArg(argc, argv, i);
      ensureArg(argc, argv, i + 1);
      auto phonemeStr = std::string(argv[++i]);
      if (!piper::isSingleCodepoint(phonemeStr)) {
        std::cerr << "Phoneme '" << phonemeStr
                  << "' is not a single codepoint (--phoneme_silence)"
                  << std::endl;
        exit(1);
      }

      if (!runConfig.phonemeSilenceSeconds) {
        runConfig.phonemeSilenceSeconds.emplace();
      }

      auto phoneme = piper::getCodepoint(phonemeStr);
      (*runConfig.phonemeSilenceSeconds)[phoneme] = stof(argv[++i]);
    } else if (arg == "--espeak_data" || arg == "--espeak-data") {
      ensureArg(argc, argv, i);
      runConfig.eSpeakDataPath = filesystem::path(argv[++i]);
    } else if (arg == "--tashkeel_model" || arg == "--tashkeel-model") {
      ensureArg(argc, argv, i);
      runConfig.tashkeelModelPath = filesystem::path(argv[++i]);
    } else if (arg == "--text") {
      ensureArg(argc, argv, i);
      runConfig.inputText = string(argv[++i]);
    } else if (arg == "--input_file" || arg == "--input-file") {
      ensureArg(argc, argv, i);
      runConfig.inputFilePath = filesystem::path(argv[++i]);
    } else if (arg == "--json_input" || arg == "--json-input") {
      runConfig.jsonInput = true;
    } else if (arg == "--use_cuda" || arg == "--use-cuda") {
      runConfig.useCuda = true;
    } else if (arg == "--server" || arg == "--serve") {
      runConfig.serverMode = true;
    } else if (arg == "--host") {
      ensureArg(argc, argv, i);
      runConfig.serverHost = string(argv[++i]);
    } else if (arg == "--port") {
      ensureArg(argc, argv, i);
      runConfig.serverPort = stoi(argv[++i]);
      if ((runConfig.serverPort < 1) || (runConfig.serverPort > 65535)) {
        throw runtime_error("--port must be between 1 and 65535");
      }
    } else if (arg == "--models") {
      ensureArg(argc, argv, i);
      runConfig.modelsDir = filesystem::path(argv[++i]);
    } else if (arg == "--api-token" || arg == "--api_token") {
      ensureArg(argc, argv, i);
      runConfig.apiToken = string(argv[++i]);
    } else if (arg == "--cpu-profile" || arg == "--cpu_profile") {
      ensureArg(argc, argv, i);
      runConfig.cpuProfile = string(argv[++i]);
      if (runConfig.cpuProfile != "auto" && runConfig.cpuProfile != "eco" &&
          runConfig.cpuProfile != "balanced" && runConfig.cpuProfile != "fast" &&
          runConfig.cpuProfile != "max") {
        throw runtime_error("--cpu-profile must be auto, eco, balanced, fast or max");
      }
    } else if (arg == "--max-concurrent-jobs" || arg == "--max_concurrent_jobs") {
      ensureArg(argc, argv, i);
      long maxJobs = stol(argv[++i]);
      if (maxJobs < 1) {
        throw runtime_error("--max-concurrent-jobs must be >= 1");
      }
      runConfig.maxConcurrentJobs = static_cast<size_t>(maxJobs);
    } else if (arg == "--chunk-workers" || arg == "--chunk_workers") {
      ensureArg(argc, argv, i);
      long workers = stol(argv[++i]);
      if (workers < 1) {
        throw runtime_error("--chunk-workers must be >= 1");
      }
      runConfig.chunkWorkers = static_cast<size_t>(workers);
    } else if (arg == "--max-model-replicas" || arg == "--max_model_replicas") {
      ensureArg(argc, argv, i);
      long maxReplicas = stol(argv[++i]);
      if (maxReplicas < 1) {
        throw runtime_error("--max-model-replicas must be >= 1");
      }
      runConfig.maxModelReplicas = static_cast<size_t>(maxReplicas);
    } else if (arg == "--queue-size" || arg == "--queue_size") {
      ensureArg(argc, argv, i);
      long queueSize = stol(argv[++i]);
      if (queueSize < 1) {
        throw runtime_error("--queue-size must be >= 1");
      }
      runConfig.queueSize = static_cast<size_t>(queueSize);
    } else if (arg == "--queue-timeout-seconds" || arg == "--queue_timeout_seconds") {
      ensureArg(argc, argv, i);
      long timeoutSeconds = stol(argv[++i]);
      if (timeoutSeconds < 1) {
        throw runtime_error("--queue-timeout-seconds must be >= 1");
      }
      runConfig.queueTimeoutSeconds = static_cast<size_t>(timeoutSeconds);
    } else if (arg == "--max-temp-bytes" || arg == "--max_temp_bytes") {
      ensureArg(argc, argv, i);
      long long tempBytes = stoll(argv[++i]);
      if (tempBytes < 0) {
        throw runtime_error("--max-temp-bytes must be >= 0");
      }
      runConfig.maxTempBytesExplicit = true;
      runConfig.maxTempBytes = static_cast<size_t>(tempBytes);
    } else if (arg == "--output-retention-seconds" || arg == "--output_retention_seconds") {
      ensureArg(argc, argv, i);
      long retentionSeconds = stol(argv[++i]);
      if (retentionSeconds < 0) {
        throw runtime_error("--output-retention-seconds must be >= 0");
      }
      runConfig.outputRetentionSeconds = static_cast<size_t>(retentionSeconds);
    } else if (arg == "--models-refresh-seconds" || arg == "--models_refresh_seconds") {
      ensureArg(argc, argv, i);
      long refreshSeconds = stol(argv[++i]);
      if (refreshSeconds < 1) {
        throw runtime_error("--models-refresh-seconds must be >= 1");
      }
      runConfig.modelsRefreshSeconds = static_cast<size_t>(refreshSeconds);
    } else if (arg == "--export-neo" || arg == "--export_neo") {
      ensureArg(argc, argv, i);
      runConfig.exportNeoPath = filesystem::path(argv[++i]);
    } else if (arg == "--neo-image" || arg == "--neo_image") {
      ensureArg(argc, argv, i);
      runConfig.neoImagePath = filesystem::path(argv[++i]);
    } else if (arg == "--neo-compression-level" || arg == "--neo_compression_level") {
      ensureArg(argc, argv, i);
      int level = stoi(argv[++i]);
      if (level < 1 || level > 22) {
        throw runtime_error("--neo-compression-level must be between 1 and 22");
      }
      runConfig.neoCompressionLevel = level;
    } else if (arg == "--cpu_threads" || arg == "--cpu-threads") {
      ensureArg(argc, argv, i);
      string cpuThreadsArg = argv[++i];
      runConfig.cpuThreadsExplicit = true;
      if (cpuThreadsArg == "auto") {
        runConfig.cpuThreads.reset();
      } else {
        runConfig.cpuThreads = stoi(cpuThreadsArg);
        if (runConfig.cpuThreads.value() < 1) {
          throw runtime_error("--cpu-threads must be >= 1 or auto");
        }
      }
    } else if (arg == "--max_text_chunk_bytes" ||
               arg == "--max-text-chunk-bytes") {
      ensureArg(argc, argv, i);
      long chunkBytes = stol(argv[++i]);
      if (chunkBytes < 512) {
        throw runtime_error("--max-text-chunk-bytes must be >= 512");
      }
      runConfig.maxTextChunkBytes = static_cast<size_t>(chunkBytes);
    } else if (arg == "--max_input_bytes" || arg == "--max-input-bytes") {
      ensureArg(argc, argv, i);
      long inputBytes = stol(argv[++i]);
      if (inputBytes < 1024) {
        throw runtime_error("--max-input-bytes must be >= 1024");
      }
      runConfig.maxInputBytes = static_cast<size_t>(inputBytes);
    } else if (arg == "--version") {
      std::cout << piper::getVersion() << std::endl;
      exit(0);
    } else if (arg == "--debug") {
      // Set DEBUG logging
      spdlog::set_level(spdlog::level::debug);
    } else if (arg == "-q" || arg == "--quiet") {
      // diable logging
      spdlog::set_level(spdlog::level::off);
    } else if (arg == "-h" || arg == "--help") {
      printUsage(argv);
      exit(0);
    }
  }

  if (runConfig.inputText && runConfig.inputFilePath) {
    throw runtime_error("Use either --text or --input-file, not both");
  }

  if (runConfig.inputFilePath) {
    ifstream inputFile(runConfig.inputFilePath->c_str(), ios::binary);
    if (!inputFile.good()) {
      throw runtime_error("Input text file doesn't exist");
    }
  }

  if (runConfig.serverMode) {
    // Be forgiving: many users naturally pass a models folder with --model.
    // In server mode, a directory supplied to --model is treated as --models.
    if (!runConfig.modelPath.empty() && filesystem::is_directory(runConfig.modelPath)) {
      runConfig.modelsDir = runConfig.modelPath;
      runConfig.modelPath.clear();
      modelConfigPath.reset();
    }

    filesystem::create_directories(runConfig.modelsDir);

    if (runConfig.modelPath.empty()) {
      auto maybeModel = piper_server::findFirstUsableModel(runConfig.modelsDir);
      if (maybeModel) {
        runConfig.modelPath = maybeModel->modelPath;
        if (!modelConfigPath && !maybeModel->isNeo) {
          modelConfigPath = maybeModel->configPath;
        }
      } else {
        throw runtime_error(
            "No usable .onnx/.neo model found in models directory: " +
            runConfig.modelsDir.string() +
            ". Copy a .neo file or a .onnx file with its .onnx.json config, "
            "or pass --models DIR / --model FILE.");
      }
    } else if (!filesystem::exists(runConfig.modelPath)) {
      auto modelInModelsDir = runConfig.modelsDir / runConfig.modelPath;
      if (filesystem::exists(modelInModelsDir)) {
        runConfig.modelPath = modelInModelsDir;
      }
    }
  }

  if (runConfig.serverMode) {
    applyAutoServerResourceConfig(runConfig);
  }

  if (runConfig.modelPath.empty()) {
    throw runtime_error("Model file is required. Use --model FILE");
  }

  // Verify model file exists
  ifstream modelFile(runConfig.modelPath.c_str(), ios::binary);
  if (!modelFile.good()) {
    throw runtime_error("Model file doesn't exist");
  }

  if (piper_neo::isNeoFile(runConfig.modelPath)) {
    runConfig.modelConfigPath.clear();
  } else if (!modelConfigPath) {
    runConfig.modelConfigPath =
        filesystem::path(runConfig.modelPath.string() + ".json");
  } else {
    runConfig.modelConfigPath = modelConfigPath.value();
  }

  // Verify model config exists for the classic .onnx format. .neo embeds it.
  if (!piper_neo::isNeoFile(runConfig.modelPath)) {
    ifstream modelConfigFile(runConfig.modelConfigPath.c_str());
    if (!modelConfigFile.good()) {
      throw runtime_error("Model config doesn't exist");
    }
  }

  if (runConfig.exportNeoPath && piper_neo::isNeoFile(runConfig.modelPath)) {
    throw runtime_error("--export-neo expects a classic .onnx model plus .onnx.json config");
  }
}

} // namespace piper_app
