#include "tts_scheduler.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <string>

#include <spdlog/spdlog.h>

#include "utils.hpp"
#include "server/jobs/chunk_worker.hpp"
#include "server/jobs/job_lifecycle.hpp"

namespace piper_server {

FairTtsScheduler::FairTtsScheduler(piper::PiperConfig &piperConfig, ModelCache &modelCache,
                 const ServerOptions &options, ServerMetrics &metrics)
    : piperConfig(piperConfig), modelCache(modelCache), options(options),
      metrics(metrics), maxJobs(std::max<std::size_t>(1, options.maxConcurrentJobs)),
      queueSize(std::max<std::size_t>(maxJobs, options.queueSize)),
      workerCount(std::max<std::size_t>(1, options.chunkWorkers)) {
  for (std::size_t i = 0; i < workerCount; ++i) {
    workers.emplace_back([this]() { workerLoop(); });
  }
}

FairTtsScheduler::~FairTtsScheduler() {
  {
    std::lock_guard<std::mutex> lock(queueMutex);
    stopping = true;
    queueCv.notify_all();
  }
  for (auto &worker : workers) {
    if (worker.joinable()) {
      worker.join();
    }
  }
}

TtsJobResult FairTtsScheduler::synthesize(const TtsJobRequest &request) {
  if (request.shouldCancel && request.shouldCancel()) {
    metrics.cancelledJobs++;
    throw std::runtime_error("synthesis_cancelled");
  }

  const auto jobStart = std::chrono::steady_clock::now();
  auto job = createJobState(request, options);

  if (job->textChunks.empty()) {
    throw std::runtime_error("missing_fields");
  }

  spdlog::info("{} TTS job queued: id={} model={} input_bytes={} chunks={}",
               nowIso8601(), job->id,
               request.requestedModel.value_or("<default>"), request.text.size(),
               job->textChunks.size());

  std::filesystem::create_directories(job->tempDir);

  {
    std::lock_guard<std::mutex> lock(queueMutex);
    if ((activeJobs + waitingJobs) >= queueSize) {
      metrics.rejectedJobs++;
      throw std::runtime_error("server_busy");
    }

    metrics.acceptedJobs++;
    if (activeJobs < maxJobs) {
      job->activated = true;
      ++activeJobs;
      activeRoundRobin.push_back(job);
    } else {
      ++waitingJobs;
      pendingJobs.push_back(job);
    }
  }
  queueCv.notify_all();

  const auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::seconds(std::max<std::size_t>(1, options.queueTimeoutSeconds));

  {
    std::unique_lock<std::mutex> lock(job->mutex);
    while (!job->done) {
      if (request.shouldCancel && request.shouldCancel()) {
        job->cancelled = true;
        job->pendingChunks = job->inFlightChunks;
        if (job->pendingChunks == 0) {
          job->done = true;
          job->cv.notify_all();
        }
        queueCv.notify_all();
      }

      if (std::chrono::steady_clock::now() > deadline && job->startedChunks == 0) {
        job->cancelled = true;
        job->failed = true;
        job->error = "server_busy";
        job->pendingChunks = 0;
        job->done = true;
        job->cv.notify_all();
        queueCv.notify_all();
      }

      job->cv.wait_for(lock, std::chrono::milliseconds(100));
    }
  }

  {
    std::lock_guard<std::mutex> lock(queueMutex);
    if (job->activated) {
      if (activeJobs > 0) {
        --activeJobs;
      }
    } else if (waitingJobs > 0) {
      --waitingJobs;
    }
    activatePendingJobsLocked();
  }
  queueCv.notify_all();

  if (job->cancelled && !job->failed) {
    cleanupJobTemp(*job, metrics);
    metrics.cancelledJobs++;
    throw std::runtime_error("synthesis_cancelled");
  }

  if (job->failed) {
    cleanupJobTemp(*job, metrics);
    metrics.failedJobs++;
    throw std::runtime_error(job->error.empty() ? "synthesis_error" : job->error);
  }

  assembleJobWav(*job);
  const auto jobEnd = std::chrono::steady_clock::now();
  const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(jobEnd - jobStart).count();
  spdlog::info("{} TTS job finished: id={} model={} chunks={} audio_seconds={} infer_seconds={} duration_ms={} file={} bytes={}",
               nowIso8601(), job->id, job->modelName, job->textChunks.size(),
               job->synthesis.audioSeconds, job->synthesis.inferSeconds,
               elapsedMs, job->fileName,
               std::filesystem::exists(job->outputPath) ? std::filesystem::file_size(job->outputPath) : 0);
  cleanupJobTemp(*job, metrics);
  metrics.completedJobs++;

  return makeJobResult(*job);
}

std::size_t FairTtsScheduler::activeJobCount() const {
  std::lock_guard<std::mutex> lock(queueMutex);
  return activeJobs;
}

std::size_t FairTtsScheduler::waitingJobCount() const {
  std::lock_guard<std::mutex> lock(queueMutex);
  return waitingJobs;
}

std::size_t FairTtsScheduler::workerTotal() const { return workerCount; }

void FairTtsScheduler::activatePendingJobsLocked() {
  while (activeJobs < maxJobs && !pendingJobs.empty()) {
    auto pending = pendingJobs.front();
    pendingJobs.pop_front();
    if (waitingJobs > 0) {
      --waitingJobs;
    }

    std::lock_guard<std::mutex> jobLock(pending->mutex);
    if (pending->done || pending->cancelled || pending->failed) {
      continue;
    }

    pending->activated = true;
    ++activeJobs;
    activeRoundRobin.push_back(pending);
  }
}

std::optional<TtsWorkItem> FairTtsScheduler::nextWork() {
  std::unique_lock<std::mutex> lock(queueMutex);
  queueCv.wait(lock, [this]() { return stopping || !activeRoundRobin.empty(); });
  if (stopping) {
    return std::nullopt;
  }

  while (!activeRoundRobin.empty()) {
    auto job = activeRoundRobin.front();
    activeRoundRobin.pop_front();

    {
      std::lock_guard<std::mutex> jobLock(job->mutex);
      if (job->cancelled || job->failed || job->nextChunk >= job->textChunks.size()) {
        continue;
      }

      const std::size_t index = job->nextChunk++;
      ++job->startedChunks;
      ++job->inFlightChunks;

      if (job->nextChunk < job->textChunks.size()) {
        activeRoundRobin.push_back(job);
      }
      return TtsWorkItem{job, index};
    }
  }

  return std::nullopt;
}

void FairTtsScheduler::workerLoop() {
  while (true) {
    auto maybeWork = nextWork();
    if (!maybeWork) {
      if (stopping) {
        return;
      }
      continue;
    }

    metrics.processingChunks++;
    synthesizeJobChunk(piperConfig, modelCache, options, metrics, maybeWork->job, maybeWork->index);
    metrics.processingChunks--;
  }
}

} // namespace piper_server
