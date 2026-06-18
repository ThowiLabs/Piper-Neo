#include "synthesis_mode.hpp"

#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include <spdlog/spdlog.h>

#include "raw_audio_output.hpp"
#include "../json.hpp"

namespace piper_app {

namespace {

using json = nlohmann::json;

std::filesystem::path timestampedOutputPath(const RunConfig &runConfig) {
  const auto now = std::chrono::system_clock::now();
  const auto timestamp =
      std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
  std::stringstream outputName;
  outputName << timestamp << ".wav";

  std::filesystem::path outputPath = runConfig.outputPath.value_or(".");
  outputPath.append(outputName.str());
  return outputPath;
}

void logSynthesisResult(const piper::SynthesisResult &result) {
  spdlog::info("Real-time factor: {} (infer={} sec, audio={} sec)",
               result.realTimeFactor, result.inferSeconds, result.audioSeconds);
}

std::unique_ptr<std::istream> openDirectInput(const RunConfig &runConfig,
                                              std::istream *&inputStream) {
  inputStream = &std::cin;

  if (runConfig.inputText) {
    auto textInput = std::make_unique<std::istringstream>(*runConfig.inputText);
    inputStream = textInput.get();
    return textInput;
  }

  if (runConfig.inputFilePath) {
    auto fileInput = std::make_unique<std::ifstream>(runConfig.inputFilePath->string(),
                                                    std::ios::binary);
    if (!fileInput->good()) {
      throw std::runtime_error("Input text file doesn't exist");
    }
    inputStream = fileInput.get();
    return fileInput;
  }

  return nullptr;
}

bool shouldUseDirectStreamMode(const RunConfig &runConfig) {
  return !runConfig.jsonInput &&
         (runConfig.inputText || runConfig.inputFilePath ||
          (runConfig.outputType == OUTPUT_FILE));
}

int runDirectStreamMode(piper::PiperConfig &piperConfig, piper::Voice &voice,
                        const RunConfig &runConfig) {
  std::filesystem::path outputPath;
  if (runConfig.outputType == OUTPUT_FILE) {
    if (!runConfig.outputPath || runConfig.outputPath->empty()) {
      throw std::runtime_error("No output path provided");
    }
    outputPath = runConfig.outputPath.value();
  } else if (runConfig.outputType == OUTPUT_DIRECTORY) {
    outputPath = timestampedOutputPath(runConfig);
  } else if (runConfig.outputType == OUTPUT_STDOUT) {
    outputPath.clear();
  } else {
    throw std::runtime_error("--input-file/--text are not supported with --output_raw");
  }

  piper::SynthesisResult result;
  std::istream *inputStream = nullptr;
  auto ownedInput = openDirectInput(runConfig, inputStream);

  if (runConfig.outputType == OUTPUT_STDOUT) {
#ifdef _WIN32
    setmode(fileno(stdout), O_BINARY);
#endif
    piper::textToWavFileFromStream(piperConfig, voice, *inputStream, std::cout,
                                   result, runConfig.maxTextChunkBytes);
  } else {
    std::ofstream audioFile(outputPath.string(), std::ios::binary);
    piper::textToWavFileFromStream(piperConfig, voice, *inputStream, audioFile,
                                   result, runConfig.maxTextChunkBytes);
    std::cout << outputPath.string() << std::endl;
  }

  logSynthesisResult(result);
  return EXIT_SUCCESS;
}

void applyJsonLineOverrides(const json &lineRoot, piper::Voice &voice,
                            OutputType &outputType,
                            std::optional<std::filesystem::path> &maybeOutputPath) {
  if (lineRoot.contains("output_file")) {
    outputType = OUTPUT_FILE;
    maybeOutputPath = std::filesystem::path(lineRoot["output_file"].get<std::string>());
  }

  if (lineRoot.contains("speaker_id")) {
    voice.synthesisConfig.speakerId = lineRoot["speaker_id"].get<piper::SpeakerId>();
    return;
  }

  if (!lineRoot.contains("speaker")) {
    return;
  }

  auto speakerName = lineRoot["speaker"].get<std::string>();
  if ((voice.modelConfig.speakerIdMap) &&
      (voice.modelConfig.speakerIdMap->count(speakerName) > 0)) {
    voice.synthesisConfig.speakerId = (*voice.modelConfig.speakerIdMap)[speakerName];
  } else {
    spdlog::warn("No speaker named: {}", speakerName);
  }
}

void writeRawAudioToStdout(piper::PiperConfig &piperConfig, piper::Voice &voice,
                           const std::string &line, piper::SynthesisResult &result) {
  std::mutex mutAudio;
  std::condition_variable cvAudio;
  bool audioReady = false;
  bool audioFinished = false;
  std::vector<int16_t> audioBuffer;
  std::vector<int16_t> sharedAudioBuffer;

#ifdef _WIN32
  setmode(fileno(stdout), O_BINARY);
  setmode(fileno(stdin), O_BINARY);
#endif

  std::thread rawOutputThread(rawOutputProc, std::ref(sharedAudioBuffer),
                              std::ref(mutAudio), std::ref(cvAudio),
                              std::ref(audioReady), std::ref(audioFinished));
  auto audioCallback = [&audioBuffer, &sharedAudioBuffer, &mutAudio, &cvAudio,
                        &audioReady]() {
    std::unique_lock lockAudio(mutAudio);
    std::copy(audioBuffer.begin(), audioBuffer.end(),
              std::back_inserter(sharedAudioBuffer));
    audioReady = true;
    cvAudio.notify_one();
  };

  piper::textToAudio(piperConfig, voice, line, audioBuffer, result, audioCallback);

  {
    std::unique_lock lockAudio(mutAudio);
    audioReady = true;
    audioFinished = true;
    cvAudio.notify_one();
  }

  spdlog::info("Waiting for audio to finish playing...");
  rawOutputThread.join();
}

void writeLineOutput(piper::PiperConfig &piperConfig, piper::Voice &voice,
                     const RunConfig &runConfig, const std::string &line,
                     OutputType outputType,
                     const std::optional<std::filesystem::path> &maybeOutputPath,
                     piper::SynthesisResult &result) {
  if (outputType == OUTPUT_DIRECTORY) {
    auto outputPath = timestampedOutputPath(runConfig);
    std::ofstream audioFile(outputPath.string(), std::ios::binary);
    piper::textToWavFile(piperConfig, voice, line, audioFile, result,
                         runConfig.maxTextChunkBytes);
    std::cout << outputPath.string() << std::endl;
  } else if (outputType == OUTPUT_FILE) {
    if (!maybeOutputPath || maybeOutputPath->empty()) {
      throw std::runtime_error("No output path provided");
    }
    auto outputPath = maybeOutputPath.value();
    std::ofstream audioFile(outputPath.string(), std::ios::binary);
    piper::textToWavFile(piperConfig, voice, line, audioFile, result,
                         runConfig.maxTextChunkBytes);
    std::cout << outputPath.string() << std::endl;
  } else if (outputType == OUTPUT_STDOUT) {
    piper::textToWavFile(piperConfig, voice, line, std::cout, result,
                         runConfig.maxTextChunkBytes);
  } else if (outputType == OUTPUT_RAW) {
    writeRawAudioToStdout(piperConfig, voice, line, result);
  }
}

} // namespace

int runSynthesisMode(piper::PiperConfig &piperConfig, piper::Voice &voice,
                     RunConfig &runConfig) {
  if (runConfig.outputType == OUTPUT_DIRECTORY) {
    runConfig.outputPath = std::filesystem::absolute(runConfig.outputPath.value());
    spdlog::info("Output directory: {}", runConfig.outputPath.value().string());
  }

  if (shouldUseDirectStreamMode(runConfig)) {
    return runDirectStreamMode(piperConfig, voice, runConfig);
  }

  std::string line;
  while (std::getline(std::cin, line)) {
    piper::SynthesisResult result;
    auto outputType = runConfig.outputType;
    auto speakerId = voice.synthesisConfig.speakerId;
    std::optional<std::filesystem::path> maybeOutputPath = runConfig.outputPath;

    if (runConfig.jsonInput) {
      json lineRoot = json::parse(line);
      line = lineRoot["text"].get<std::string>();
      applyJsonLineOverrides(lineRoot, voice, outputType, maybeOutputPath);
    }

    writeLineOutput(piperConfig, voice, runConfig, line, outputType, maybeOutputPath,
                    result);
    logSynthesisResult(result);

    voice.synthesisConfig.speakerId = speakerId;
  }

  return EXIT_SUCCESS;
}

} // namespace piper_app
