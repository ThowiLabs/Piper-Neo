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
#include "server/jobs/job_state.hpp"
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
  void activatePendingJobsLocked();
  std::optional<TtsWorkItem> nextWork();
  void workerLoop();

  piper::PiperConfig &piperConfig;
  ModelCache &modelCache;
  const ServerOptions &options;
  ServerMetrics &metrics;
  const std::size_t maxJobs;
  const std::size_t queueSize;
  const std::size_t workerCount;
  mutable std::mutex queueMutex;
  std::condition_variable queueCv;
  std::deque<std::shared_ptr<TtsJobState>> activeRoundRobin;
  std::deque<std::shared_ptr<TtsJobState>> pendingJobs;
  std::vector<std::thread> workers;
  bool stopping = false;
  std::size_t activeJobs = 0;
  std::size_t waitingJobs = 0;
};

} // namespace piper_server

#endif // PIPER_SERVER_TTS_SCHEDULER_H_
