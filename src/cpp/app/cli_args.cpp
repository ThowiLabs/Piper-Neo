#include "cli_args.hpp"

#include "cli_validation.hpp"
#include "help_text.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

#include <spdlog/spdlog.h>

#include "../piper.hpp"

namespace piper_app {

using namespace std;

namespace {

void ensureArg(int argc, char *argv[], int argi) {
  if ((argi + 1) >= argc) {
    printUsage(argv);
    exit(0);
  }
}

} // namespace

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

  finalizeRunConfig(runConfig, modelConfigPath);
}

} // namespace piper_app
