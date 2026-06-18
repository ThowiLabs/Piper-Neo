#include "piper_app.hpp"

#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "cli_args.hpp"
#include "env.hpp"
#include "hardware.hpp"
#include "platform.hpp"
#include "raw_audio_output.hpp"
#include "run_config.hpp"
#include "../json.hpp"
#include "../neo_model.hpp"
#include "../piper.hpp"
#include "../server.hpp"

namespace piper_app {

using namespace std;
using json = nlohmann::json;

int piperMain(int argc, char *argv[]) {
  spdlog::set_default_logger(spdlog::stderr_color_st("piper"));

  RunConfig runConfig;
  parseArgs(argc, argv, runConfig);

  if (runConfig.exportNeoPath) {
    const auto start = chrono::steady_clock::now();
    spdlog::info("Exporting Piper Neo package: output={} compression=zstd level={}",
                 runConfig.exportNeoPath->string(), runConfig.neoCompressionLevel);
    piper_neo::writePackageFromOnnx(runConfig.modelPath, runConfig.modelConfigPath,
                                    *runConfig.exportNeoPath, runConfig.neoImagePath,
                                    runConfig.neoCompressionLevel);
    const auto end = chrono::steady_clock::now();
    spdlog::info("Piper Neo package exported in {} ms",
                 chrono::duration_cast<chrono::milliseconds>(end - start).count());
    return EXIT_SUCCESS;
  }

  configureConsoleUtf8();

  piper::PiperConfig piperConfig;
  piper::Voice voice;

  filesystem::path loadModelPath = runConfig.modelPath;
  filesystem::path loadConfigPath = runConfig.modelConfigPath;
  optional<piper_neo::ExtractedNeoModel> extractedNeoModel;
  if (piper_neo::isNeoFile(runConfig.modelPath)) {
    extractedNeoModel = piper_neo::extractPackage(
        runConfig.modelPath,
        filesystem::temp_directory_path() / "piper-neo-cli-cache");
    loadModelPath = extractedNeoModel->modelPath;
    loadConfigPath = extractedNeoModel->configPath;
  }

  spdlog::debug("Loading voice from {} (config={})",
                loadModelPath.string(),
                loadConfigPath.string());

  const auto loadWallTime = chrono::system_clock::now();
  const auto loadWallT = chrono::system_clock::to_time_t(loadWallTime);
  std::tm loadWallTm{};
#ifdef _WIN32
  gmtime_s(&loadWallTm, &loadWallT);
#else
  gmtime_r(&loadWallT, &loadWallTm);
#endif
  std::ostringstream loadTimestamp;
  loadTimestamp << std::put_time(&loadWallTm, "%Y-%m-%dT%H:%M:%SZ");
  spdlog::info("{} Model load started: model={} format={}", loadTimestamp.str(),
               runConfig.modelPath.filename().string(),
               piper_neo::isNeoFile(runConfig.modelPath) ? "neo" : "onnx");

  auto startTime = chrono::steady_clock::now();
  loadVoice(piperConfig, loadModelPath.string(),
            loadConfigPath.string(), voice, runConfig.speakerId,
            runConfig.useCuda, runConfig.cpuThreads);
  auto endTime = chrono::steady_clock::now();
  spdlog::info("{} Model load finished: model={} format={} duration_ms={}",
               loadTimestamp.str(), runConfig.modelPath.filename().string(),
               piper_neo::isNeoFile(runConfig.modelPath) ? "neo" : "onnx",
               chrono::duration_cast<chrono::milliseconds>(endTime - startTime).count());

  const auto exePath = resolveExecutablePath(argv[0]);

  if (voice.phonemizeConfig.phonemeType == piper::eSpeakPhonemes) {
    spdlog::debug("Voice uses eSpeak phonemes ({})",
                  voice.phonemizeConfig.eSpeak.voice);

    if (runConfig.eSpeakDataPath) {
      // User provided path
      piperConfig.eSpeakDataPath = runConfig.eSpeakDataPath.value().string();
    } else {
      // Assume next to piper executable
      piperConfig.eSpeakDataPath =
          std::filesystem::absolute(
              exePath.parent_path().append("espeak-ng-data"))
              .string();

      spdlog::debug("espeak-ng-data directory is expected at {}",
                    piperConfig.eSpeakDataPath);
    }
  } else {
    // Not using eSpeak
    piperConfig.useESpeak = false;
  }

  // Enable libtashkeel for Arabic
  if (voice.phonemizeConfig.eSpeak.voice == "ar") {
    piperConfig.useTashkeel = true;
    if (runConfig.tashkeelModelPath) {
      // User provided path
      piperConfig.tashkeelModelPath =
          runConfig.tashkeelModelPath.value().string();
    } else {
      // Assume next to piper executable
      piperConfig.tashkeelModelPath =
          std::filesystem::absolute(
              exePath.parent_path().append("libtashkeel_model.ort"))
              .string();

      spdlog::debug("libtashkeel model is expected at {}",
                    piperConfig.tashkeelModelPath.value());
    }
  }

  piper::initialize(piperConfig);

  // Scales
  if (runConfig.noiseScale) {
    voice.synthesisConfig.noiseScale = runConfig.noiseScale.value();
  }

  if (runConfig.lengthScale) {
    voice.synthesisConfig.lengthScale = runConfig.lengthScale.value();
  }

  if (runConfig.noiseW) {
    voice.synthesisConfig.noiseW = runConfig.noiseW.value();
  }

  if (runConfig.sentenceSilenceSeconds) {
    voice.synthesisConfig.sentenceSilenceSeconds =
        runConfig.sentenceSilenceSeconds.value();
  }

  if (runConfig.phonemeSilenceSeconds) {
    if (!voice.synthesisConfig.phonemeSilenceSeconds) {
      // Overwrite
      voice.synthesisConfig.phonemeSilenceSeconds =
          runConfig.phonemeSilenceSeconds;
    } else {
      // Merge
      for (const auto &[phoneme, silenceSeconds] :
           *runConfig.phonemeSilenceSeconds) {
        voice.synthesisConfig.phonemeSilenceSeconds->try_emplace(
            phoneme, silenceSeconds);
      }
    }

  } // if phonemeSilenceSeconds

  if (runConfig.outputType == OUTPUT_DIRECTORY) {
    runConfig.outputPath = filesystem::absolute(runConfig.outputPath.value());
    spdlog::info("Output directory: {}", runConfig.outputPath.value().string());
  }

  auto timestampedOutputPath = [&runConfig]() {
    const auto now = chrono::system_clock::now();
    const auto timestamp =
        chrono::duration_cast<chrono::nanoseconds>(now.time_since_epoch())
            .count();
    stringstream outputName;
    outputName << timestamp << ".wav";
    filesystem::path outputPath = runConfig.outputPath.value_or(".");
    outputPath.append(outputName.str());
    return outputPath;
  };

  if (runConfig.serverMode) {
    piper_server::ServerOptions serverOptions;
    serverOptions.host = runConfig.serverHost;
    serverOptions.port = runConfig.serverPort;
    serverOptions.modelsDir = filesystem::absolute(runConfig.modelsDir);
    serverOptions.outputDir = filesystem::absolute(
        runConfig.outputDirExplicit ? runConfig.outputPath.value()
                                    : filesystem::path("outputs"));
    serverOptions.activeModelPath = filesystem::absolute(runConfig.modelPath);
    serverOptions.activeModelConfigPath = runConfig.modelConfigPath.empty()
                                            ? filesystem::path()
                                            : filesystem::absolute(runConfig.modelConfigPath);
    serverOptions.defaultSpeakerId = runConfig.speakerId;
    serverOptions.useCuda = runConfig.useCuda;
    serverOptions.cpuThreads = runConfig.cpuThreads;
    serverOptions.maxInputBytes = runConfig.maxInputBytes;
    serverOptions.maxTextChunkBytes = runConfig.maxTextChunkBytes;
    serverOptions.maxConcurrentJobs = runConfig.maxConcurrentJobs;
    serverOptions.maxModelReplicas = runConfig.maxModelReplicas;
    serverOptions.chunkWorkers = runConfig.chunkWorkers;
    serverOptions.queueSize = runConfig.queueSize;
    serverOptions.queueTimeoutSeconds = runConfig.queueTimeoutSeconds;
    serverOptions.maxTempBytes = runConfig.maxTempBytes;
    serverOptions.outputRetentionSeconds = runConfig.outputRetentionSeconds;
    serverOptions.modelsRefreshSeconds = runConfig.modelsRefreshSeconds;
    serverOptions.cpuProfile = runConfig.cpuProfile;
    serverOptions.detectedHardwareThreads = runConfig.detectedHardwareThreads;
    serverOptions.detectedMemoryBytes = runConfig.detectedMemoryBytes;
    serverOptions.apiToken = resolveApiToken(runConfig.apiToken).value_or("");

    piper_server::runServer(piperConfig, voice, serverOptions);
    piper::terminate(piperConfig);
    return EXIT_SUCCESS;
  }

  if (!runConfig.jsonInput && (runConfig.inputText || runConfig.inputFilePath ||
                               (runConfig.outputType == OUTPUT_FILE))) {
    filesystem::path outputPath;
    if (runConfig.outputType == OUTPUT_FILE) {
      if (!runConfig.outputPath || runConfig.outputPath->empty()) {
        throw runtime_error("No output path provided");
      }
      outputPath = runConfig.outputPath.value();
    } else if (runConfig.outputType == OUTPUT_DIRECTORY) {
      outputPath = timestampedOutputPath();
    } else if (runConfig.outputType == OUTPUT_STDOUT) {
      outputPath.clear();
    } else {
      throw runtime_error("--input-file/--text are not supported with --output_raw");
    }

    piper::SynthesisResult result;
    std::unique_ptr<std::istream> ownedInput;
    std::istream *inputStream = &cin;

    if (runConfig.inputText) {
      ownedInput = std::make_unique<std::istringstream>(*runConfig.inputText);
      inputStream = ownedInput.get();
    } else if (runConfig.inputFilePath) {
      auto fileInput = std::make_unique<std::ifstream>(
          runConfig.inputFilePath->string(), ios::binary);
      if (!fileInput->good()) {
        throw runtime_error("Input text file doesn't exist");
      }
      inputStream = fileInput.get();
      ownedInput = std::move(fileInput);
    }

    if (runConfig.outputType == OUTPUT_STDOUT) {
#ifdef _WIN32
      setmode(fileno(stdout), O_BINARY);
#endif
      piper::textToWavFileFromStream(piperConfig, voice, *inputStream, cout,
                                     result, runConfig.maxTextChunkBytes);
    } else {
      ofstream audioFile(outputPath.string(), ios::binary);
      piper::textToWavFileFromStream(piperConfig, voice, *inputStream, audioFile,
                                     result, runConfig.maxTextChunkBytes);
      cout << outputPath.string() << endl;
    }

    spdlog::info("Real-time factor: {} (infer={} sec, audio={} sec)",
                 result.realTimeFactor, result.inferSeconds,
                 result.audioSeconds);

    piper::terminate(piperConfig);
    return EXIT_SUCCESS;
  }

  string line;
  while (getline(cin, line)) {
    piper::SynthesisResult result;
    auto outputType = runConfig.outputType;
    auto speakerId = voice.synthesisConfig.speakerId;
    std::optional<filesystem::path> maybeOutputPath = runConfig.outputPath;

    if (runConfig.jsonInput) {
      // Each line is a JSON object
      json lineRoot = json::parse(line);

      // Text is required
      line = lineRoot["text"].get<std::string>();

      if (lineRoot.contains("output_file")) {
        // Override output WAV file path
        outputType = OUTPUT_FILE;
        maybeOutputPath =
            filesystem::path(lineRoot["output_file"].get<std::string>());
      }

      if (lineRoot.contains("speaker_id")) {
        // Override speaker id
        voice.synthesisConfig.speakerId =
            lineRoot["speaker_id"].get<piper::SpeakerId>();
      } else if (lineRoot.contains("speaker")) {
        // Resolve to id using speaker id map
        auto speakerName = lineRoot["speaker"].get<std::string>();
        if ((voice.modelConfig.speakerIdMap) &&
            (voice.modelConfig.speakerIdMap->count(speakerName) > 0)) {
          voice.synthesisConfig.speakerId =
              (*voice.modelConfig.speakerIdMap)[speakerName];
        } else {
          spdlog::warn("No speaker named: {}", speakerName);
        }
      }
    }

    // Timestamp is used for path to output WAV file
    const auto now = chrono::system_clock::now();
    const auto timestamp =
        chrono::duration_cast<chrono::nanoseconds>(now.time_since_epoch())
            .count();

    if (outputType == OUTPUT_DIRECTORY) {
      // Generate path using timestamp
      stringstream outputName;
      outputName << timestamp << ".wav";
      filesystem::path outputPath = runConfig.outputPath.value();
      outputPath.append(outputName.str());

      // Output audio to automatically-named WAV file in a directory
      ofstream audioFile(outputPath.string(), ios::binary);
      piper::textToWavFile(piperConfig, voice, line, audioFile, result,
                            runConfig.maxTextChunkBytes);
      cout << outputPath.string() << endl;
    } else if (outputType == OUTPUT_FILE) {
      if (!maybeOutputPath || maybeOutputPath->empty()) {
        throw runtime_error("No output path provided");
      }

      filesystem::path outputPath = maybeOutputPath.value();

      // Output audio to WAV file
      ofstream audioFile(outputPath.string(), ios::binary);
      piper::textToWavFile(piperConfig, voice, line, audioFile, result,
                            runConfig.maxTextChunkBytes);
      cout << outputPath.string() << endl;
    } else if (outputType == OUTPUT_STDOUT) {
      // Output WAV to stdout
      piper::textToWavFile(piperConfig, voice, line, cout, result,
                            runConfig.maxTextChunkBytes);
    } else if (outputType == OUTPUT_RAW) {
      // Raw output to stdout
      mutex mutAudio;
      condition_variable cvAudio;
      bool audioReady = false;
      bool audioFinished = false;
      vector<int16_t> audioBuffer;
      vector<int16_t> sharedAudioBuffer;

#ifdef _WIN32
      // Needed on Windows to avoid terminal conversions
      setmode(fileno(stdout), O_BINARY);
      setmode(fileno(stdin), O_BINARY);
#endif

      thread rawOutputThread(rawOutputProc, ref(sharedAudioBuffer),
                             ref(mutAudio), ref(cvAudio), ref(audioReady),
                             ref(audioFinished));
      auto audioCallback = [&audioBuffer, &sharedAudioBuffer, &mutAudio,
                            &cvAudio, &audioReady]() {
        // Signal thread that audio is ready
        {
          unique_lock lockAudio(mutAudio);
          copy(audioBuffer.begin(), audioBuffer.end(),
               back_inserter(sharedAudioBuffer));
          audioReady = true;
          cvAudio.notify_one();
        }
      };
      piper::textToAudio(piperConfig, voice, line, audioBuffer, result,
                         audioCallback);

      // Signal thread that there is no more audio
      {
        unique_lock lockAudio(mutAudio);
        audioReady = true;
        audioFinished = true;
        cvAudio.notify_one();
      }

      // Wait for audio output to finish
      spdlog::info("Waiting for audio to finish playing...");
      rawOutputThread.join();
    }

    spdlog::info("Real-time factor: {} (infer={} sec, audio={} sec)",
                 result.realTimeFactor, result.inferSeconds,
                 result.audioSeconds);

    // Restore config (--json-input)
    voice.synthesisConfig.speakerId = speakerId;

  } // for each line

  piper::terminate(piperConfig);

  return EXIT_SUCCESS;
}


} // namespace piper_app
