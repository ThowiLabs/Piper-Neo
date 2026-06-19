#include "synthesis_mode.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include <spdlog/spdlog.h>

#include "synthesis_input.hpp"
#include "synthesis_json.hpp"
#include "synthesis_output.hpp"
#include "synthesis_paths.hpp"
#include "../json.hpp"

namespace piper_app {

namespace {

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
      nlohmann::json lineRoot = nlohmann::json::parse(line);
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
