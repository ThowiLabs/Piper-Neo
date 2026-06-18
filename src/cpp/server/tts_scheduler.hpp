#ifndef PIPER_SERVER_TTS_SCHEDULER_H_
#define PIPER_SERVER_TTS_SCHEDULER_H_

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#include "../server.hpp"
#include "model_cache.hpp"
#include "types.hpp"

namespace piper_server {

class FairTtsScheduler {
public:
  FairTtsScheduler(piper::PiperConfig &piperConfig, ModelCache &modelCache,
                   const ServerOptions &options, ServerMetrics &metrics);
  ~FairTtsScheduler();

  TtsJobResult synthesize(const TtsJobRequest &request);
  std::size_t activeJobCount() const;
  std::size_t waitingJobCount() const;
  std::size_t workerTotal() const;

private:
  struct JobState {
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

  struct WorkItem {
    std::shared_ptr<JobState> job;
    std::size_t index = 0;
  };

  void activatePendingJobsLocked();
  std::optional<WorkItem> nextWork();
  void workerLoop();
  void processChunk(const std::shared_ptr<JobState> &job, std::size_t index);
  void markChunkFinished(JobState &job);
  void assembleWav(const JobState &job);
  void cleanupJob(JobState &job);

  piper::PiperConfig &piperConfig;
  ModelCache &modelCache;
  const ServerOptions &options;
  ServerMetrics &metrics;
  const std::size_t maxJobs;
  const std::size_t queueSize;
  const std::size_t workerCount;
  mutable std::mutex queueMutex;
  std::condition_variable queueCv;
  std::deque<std::shared_ptr<JobState>> activeRoundRobin;
  std::deque<std::shared_ptr<JobState>> pendingJobs;
  std::vector<std::thread> workers;
  bool stopping = false;
  std::size_t activeJobs = 0;
  std::size_t waitingJobs = 0;
};

json resourcePolicyJson(const ServerOptions &options, const FairTtsScheduler &scheduler);
json metricsJson(const ServerMetrics &metrics, const FairTtsScheduler &scheduler,
                 const ServerOptions &options);

} // namespace piper_server

#endif // PIPER_SERVER_TTS_SCHEDULER_H_
