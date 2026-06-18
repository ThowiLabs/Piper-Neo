#ifndef PIPER_SERVER_TYPES_H_
#define PIPER_SERVER_TYPES_H_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "../json.hpp"
#include "../piper.hpp"
#include "sanitize_result.hpp"

namespace piper_server {

using json = nlohmann::json;

struct HttpRequest {
  std::string method;
  std::string path;
  std::string body;
  std::map<std::string, std::string> headers;
};

struct ParsedTarget {
  std::string path;
  std::map<std::string, std::string> query;
};

struct ServerMetrics {
  std::atomic<std::uint64_t> acceptedJobs{0};
  std::atomic<std::uint64_t> completedJobs{0};
  std::atomic<std::uint64_t> cancelledJobs{0};
  std::atomic<std::uint64_t> failedJobs{0};
  std::atomic<std::uint64_t> rejectedJobs{0};
  std::atomic<std::uint64_t> completedChunks{0};
  std::atomic<std::uint64_t> failedChunks{0};
  std::atomic<std::uint64_t> processingChunks{0};
  std::atomic<std::uint64_t> tempStorageBytes{0};
  std::atomic<std::uint64_t> sanitizedInputs{0};
  std::atomic<std::uint64_t> sanitizeWarnings{0};
  std::atomic<std::uint64_t> rejectedTextInputs{0};
};

struct TtsJobResult {
  std::string fileName;
  std::filesystem::path outputPath;
  std::string modelName;
  std::filesystem::path modelPath;
  std::size_t chunks = 0;
  std::uintmax_t bytes = 0;
  piper::SynthesisResult synthesis;
};

struct TtsJobRequest {
  std::string text;
  std::string fileName;
  std::filesystem::path outputPath;
  std::optional<std::string> requestedModel;
  std::optional<piper::SpeakerId> speakerId;
  std::optional<float> noiseScale;
  std::optional<float> lengthScale;
  std::optional<float> noiseW;
  std::optional<float> sentenceSilenceSeconds;
  std::function<bool()> shouldCancel;
};

struct MarkupVoiceSettings {
  std::optional<std::string> model;
  std::optional<piper::SpeakerId> speakerId;
  std::optional<float> noiseScale;
  std::optional<float> lengthScale;
  std::optional<float> noiseW;
  std::optional<float> sentenceSilenceSeconds;
};

struct MarkupSegment {
  enum class Type { Speech, Silence };
  Type type = Type::Speech;
  std::string text;
  std::uint64_t silenceMs = 0;
  MarkupVoiceSettings voice;
};

struct MarkupParseResult {
  std::vector<MarkupSegment> segments;
  std::vector<std::string> warnings;
};

struct MarkupRenderResult {
  TtsJobResult tts;
  json segments = json::array();
  std::size_t speechSegments = 0;
  std::size_t silenceSegments = 0;
  int outputSampleRate = 0;
};

} // namespace piper_server

#endif // PIPER_SERVER_TYPES_H_
