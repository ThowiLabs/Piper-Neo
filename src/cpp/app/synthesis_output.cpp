#include "synthesis_output.hpp"

#include <condition_variable>
#include <fstream>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include <spdlog/spdlog.h>

#include "raw_audio_output.hpp"
#include "synthesis_paths.hpp"

namespace piper_app {

void logSynthesisResult(const piper::SynthesisResult &result) {
  spdlog::info("Real-time factor: {} (infer={} sec, audio={} sec)",
               result.realTimeFactor, result.inferSeconds, result.audioSeconds);
}

namespace {

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

void writeWavFile(piper::PiperConfig &piperConfig, piper::Voice &voice,
                  const std::string &line, const std::filesystem::path &outputPath,
                  piper::SynthesisResult &result, std::size_t maxTextChunkBytes) {
  std::ofstream audioFile(outputPath.string(), std::ios::binary);
  piper::textToWavFile(piperConfig, voice, line, audioFile, result,
                       maxTextChunkBytes);
  std::cout << outputPath.string() << std::endl;
}

} // namespace

void writeLineOutput(piper::PiperConfig &piperConfig, piper::Voice &voice,
                     const RunConfig &runConfig, const std::string &line,
                     OutputType outputType,
                     const std::optional<std::filesystem::path> &maybeOutputPath,
                     piper::SynthesisResult &result) {
  if (outputType == OUTPUT_DIRECTORY) {
    writeWavFile(piperConfig, voice, line, timestampedOutputPath(runConfig),
                 result, runConfig.maxTextChunkBytes);
  } else if (outputType == OUTPUT_FILE) {
    if (!maybeOutputPath || maybeOutputPath->empty()) {
      throw std::runtime_error("No output path provided");
    }
    writeWavFile(piperConfig, voice, line, maybeOutputPath.value(), result,
                 runConfig.maxTextChunkBytes);
  } else if (outputType == OUTPUT_STDOUT) {
    piper::textToWavFile(piperConfig, voice, line, std::cout, result,
                         runConfig.maxTextChunkBytes);
  } else if (outputType == OUTPUT_RAW) {
    writeRawAudioToStdout(piperConfig, voice, line, result);
  }
}

} // namespace piper_app
