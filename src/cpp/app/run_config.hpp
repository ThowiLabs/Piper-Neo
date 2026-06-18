#ifndef PIPER_APP_RUN_CONFIG_HPP_
#define PIPER_APP_RUN_CONFIG_HPP_

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>

#include "../piper.hpp"

namespace piper_app {

enum OutputType { OUTPUT_FILE, OUTPUT_DIRECTORY, OUTPUT_STDOUT, OUTPUT_RAW };

struct RunConfig {
  // Path to .onnx/.neo voice file.
  std::filesystem::path modelPath;

  // Path to JSON voice config file. Empty when the selected model is .neo.
  std::filesystem::path modelConfigPath;

  // Type of output to produce. Default: WAV file in the current directory.
  OutputType outputType = OUTPUT_DIRECTORY;
  std::optional<std::filesystem::path> outputPath = std::filesystem::path(".");

  std::optional<piper::SpeakerId> speakerId;
  std::optional<float> noiseScale;
  std::optional<float> lengthScale;
  std::optional<float> noiseW;
  std::optional<float> sentenceSilenceSeconds;
  std::optional<std::filesystem::path> eSpeakDataPath;
  std::optional<std::filesystem::path> tashkeelModelPath;

  bool jsonInput = false;
  std::optional<std::map<piper::Phoneme, float>> phonemeSilenceSeconds;
  bool useCuda = false;

  // 0/nullopt lets ONNX Runtime choose its default thread count.
  std::optional<int> cpuThreads;
  bool cpuThreadsExplicit = false;

  std::size_t maxTextChunkBytes = 4096;
  std::size_t maxInputBytes = 10 * 1024 * 1024;

  std::optional<std::string> inputText;
  std::optional<std::filesystem::path> inputFilePath;

  bool serverMode = false;
  std::string serverHost = "127.0.0.1";
  int serverPort = 8080;
  std::filesystem::path modelsDir = std::filesystem::path("models");
  std::optional<std::string> apiToken;
  std::size_t maxConcurrentJobs = 0;
  std::size_t maxModelReplicas = 0;
  std::size_t chunkWorkers = 0;
  std::size_t queueSize = 0;
  std::size_t queueTimeoutSeconds = 60;
  std::size_t maxTempBytes = 0;
  bool maxTempBytesExplicit = false;
  std::size_t outputRetentionSeconds = 3600;
  std::size_t modelsRefreshSeconds = 30;
  std::string cpuProfile = "auto";
  unsigned int detectedHardwareThreads = 0;
  std::uint64_t detectedMemoryBytes = 0;
  bool outputDirExplicit = false;

  std::optional<std::filesystem::path> exportNeoPath;
  std::optional<std::filesystem::path> neoImagePath;
  int neoCompressionLevel = 10;
};

} // namespace piper_app

#endif // PIPER_APP_RUN_CONFIG_HPP_
