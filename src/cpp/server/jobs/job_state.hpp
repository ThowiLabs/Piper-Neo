#ifndef PIPER_SERVER_JOBS_JOB_STATE_H_
#define PIPER_SERVER_JOBS_JOB_STATE_H_

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "server/types.hpp"

namespace piper_server {

struct TtsJobState {
  std::string id;
  std::string fileName;
  std::filesystem::path outputPath;
  std::filesystem::path tempDir;
  std::optional<std::string> requestedModel;
  std::optional<piper::SpeakerId> speakerId;
  std::optional<float> noiseScale;
  std::optional<float> lengthScale;
  std::optional<float> noiseW;
  std::optional<float> sentenceSilenceSeconds;
  std::function<bool()> shouldCancel;
  std::vector<std::string> textChunks;
  std::vector<std::filesystem::path> chunkPaths;
  std::vector<std::uintmax_t> chunkBytes;
  std::atomic<std::uint64_t> allocatedTempBytes{0};
  std::size_t nextChunk = 0;
  std::size_t pendingChunks = 0;
  std::size_t startedChunks = 0;
  std::size_t inFlightChunks = 0;
  bool activated = false;
  bool cancelled = false;
  bool failed = false;
  bool done = false;
  std::string error;
  std::string modelName;
  std::filesystem::path modelPath;
  int sampleRate = 22050;
  int sampleWidth = 2;
  int channels = 1;
  piper::SynthesisResult synthesis;
  std::mutex mutex;
  std::condition_variable cv;
};

struct TtsWorkItem {
  std::shared_ptr<TtsJobState> job;
  std::size_t index = 0;
};

} // namespace piper_server

#endif // PIPER_SERVER_JOBS_JOB_STATE_H_
