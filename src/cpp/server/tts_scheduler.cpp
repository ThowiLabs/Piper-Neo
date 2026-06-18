#include "tts_scheduler.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>

#include <spdlog/spdlog.h>

#include "utils.hpp"
#include "wav_utils.hpp"

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
  auto job = std::make_shared<JobState>();
  job->id = makeOutputFileName();
  if (job->id.size() > 4 && job->id.substr(job->id.size() - 4) == ".wav") {
    job->id.erase(job->id.size() - 4);
  }
  job->textChunks = piper::splitTextIntoChunks(request.text, options.maxTextChunkBytes);
  job->fileName = request.fileName;
  job->outputPath = request.outputPath;
  job->requestedModel = request.requestedModel;
  job->speakerId = request.speakerId;
  job->noiseScale = request.noiseScale;
  job->lengthScale = request.lengthScale;
  job->noiseW = request.noiseW;
  job->sentenceSilenceSeconds = request.sentenceSilenceSeconds;
  job->shouldCancel = request.shouldCancel;
  job->pendingChunks = job->textChunks.size();
  job->chunkPaths.resize(job->textChunks.size());
  job->chunkBytes.resize(job->textChunks.size(), 0);
  job->tempDir = options.outputDir / "tmp" / job->id;

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
    cleanupJob(*job);
    metrics.cancelledJobs++;
    throw std::runtime_error("synthesis_cancelled");
  }

  if (job->failed) {
    cleanupJob(*job);
    metrics.failedJobs++;
    throw std::runtime_error(job->error.empty() ? "synthesis_error" : job->error);
  }

  assembleWav(*job);
  const auto jobEnd = std::chrono::steady_clock::now();
  const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(jobEnd - jobStart).count();
  spdlog::info("{} TTS job finished: id={} model={} chunks={} audio_seconds={} infer_seconds={} duration_ms={} file={} bytes={}",
               nowIso8601(), job->id, job->modelName, job->textChunks.size(),
               job->synthesis.audioSeconds, job->synthesis.inferSeconds,
               elapsedMs, job->fileName,
               std::filesystem::exists(job->outputPath) ? std::filesystem::file_size(job->outputPath) : 0);
  cleanupJob(*job);
  metrics.completedJobs++;

  TtsJobResult result;
  result.fileName = job->fileName;
  result.outputPath = job->outputPath;
  result.modelName = job->modelName;
  result.modelPath = job->modelPath;
  result.chunks = job->textChunks.size();
  result.synthesis = job->synthesis;
  if (std::filesystem::exists(job->outputPath)) {
    result.bytes = std::filesystem::file_size(job->outputPath);
  }
  return result;
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

std::optional<FairTtsScheduler::WorkItem> FairTtsScheduler::nextWork() {
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
      return WorkItem{job, index};
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
    processChunk(maybeWork->job, maybeWork->index);
    metrics.processingChunks--;
  }
}

void FairTtsScheduler::processChunk(const std::shared_ptr<JobState> &job, std::size_t index) {
  try {
    {
      std::lock_guard<std::mutex> lock(job->mutex);
      if (job->cancelled || job->failed) {
        if (job->pendingChunks > job->inFlightChunks) {
          job->pendingChunks = job->inFlightChunks;
        }
        markChunkFinished(*job);
        return;
      }
    }

    auto shouldCancel = [job]() {
      std::lock_guard<std::mutex> lock(job->mutex);
      if (job->cancelled || job->failed) {
        return true;
      }
      return job->shouldCancel ? job->shouldCancel() : false;
    };

    if (shouldCancel()) {
      throw std::runtime_error("synthesis_cancelled");
    }

    VoiceLease voiceLease = modelCache.checkout(job->requestedModel);
    if (shouldCancel()) {
      throw std::runtime_error("synthesis_cancelled");
    }

    auto &selectedVoice = voiceLease.get();
    const auto previousSpeakerId = selectedVoice.synthesisConfig.speakerId;
    const auto previousNoiseScale = selectedVoice.synthesisConfig.noiseScale;
    const auto previousLengthScale = selectedVoice.synthesisConfig.lengthScale;
    const auto previousNoiseW = selectedVoice.synthesisConfig.noiseW;
    const auto previousSentenceSilence = selectedVoice.synthesisConfig.sentenceSilenceSeconds;
    auto restoreSynthesisConfig = [&]() {
      selectedVoice.synthesisConfig.speakerId = previousSpeakerId;
      selectedVoice.synthesisConfig.noiseScale = previousNoiseScale;
      selectedVoice.synthesisConfig.lengthScale = previousLengthScale;
      selectedVoice.synthesisConfig.noiseW = previousNoiseW;
      selectedVoice.synthesisConfig.sentenceSilenceSeconds = previousSentenceSilence;
    };

    if (job->speakerId) {
      selectedVoice.synthesisConfig.speakerId = *job->speakerId;
    }
    if (job->noiseScale) {
      selectedVoice.synthesisConfig.noiseScale = *job->noiseScale;
    }
    if (job->lengthScale) {
      selectedVoice.synthesisConfig.lengthScale = *job->lengthScale;
    }
    if (job->noiseW) {
      selectedVoice.synthesisConfig.noiseW = *job->noiseW;
    }
    if (job->sentenceSilenceSeconds) {
      selectedVoice.synthesisConfig.sentenceSilenceSeconds = *job->sentenceSilenceSeconds;
    }

    const auto chunkPath = job->tempDir / ("chunk_" + std::to_string(index) + ".raw");
    if (shouldCancel()) {
      restoreSynthesisConfig();
      throw std::runtime_error("synthesis_cancelled");
    }

    std::ofstream chunkFile(chunkPath, std::ios::binary);
    if (!chunkFile.good()) {
      restoreSynthesisConfig();
      throw std::runtime_error("Could not open chunk temp file");
    }

    piper::SynthesisResult chunkResult;
    std::vector<int16_t> audioBuffer;
    std::uintmax_t bytes = 0;
    auto audioCallback = [&]() {
      if (audioBuffer.empty()) {
        return;
      }
      const std::size_t audioBytes = sizeof(int16_t) * audioBuffer.size();
      chunkFile.write(reinterpret_cast<const char *>(audioBuffer.data()), audioBytes);
      bytes += audioBytes;

      const auto newTempTotal = metrics.tempStorageBytes.fetch_add(audioBytes) + audioBytes;
      job->allocatedTempBytes.fetch_add(audioBytes);
      if (options.maxTempBytes > 0 && newTempTotal > options.maxTempBytes) {
        throw std::runtime_error("temp_storage_full");
      }
    };

    try {
      piper::textToAudio(piperConfig, selectedVoice, job->textChunks[index], audioBuffer,
                         chunkResult, audioCallback, shouldCancel);
    } catch (...) {
      restoreSynthesisConfig();
      throw;
    }
    restoreSynthesisConfig();
    chunkFile.close();

    {
      std::lock_guard<std::mutex> lock(job->mutex);
      job->chunkPaths[index] = chunkPath;
      job->chunkBytes[index] = bytes;
      job->sampleRate = selectedVoice.synthesisConfig.sampleRate;
      job->sampleWidth = selectedVoice.synthesisConfig.sampleWidth;
      job->channels = selectedVoice.synthesisConfig.channels;
      job->modelName = voiceLease.model().name;
      job->modelPath = voiceLease.model().modelPath;
      job->synthesis.audioSeconds += chunkResult.audioSeconds;
      job->synthesis.inferSeconds += chunkResult.inferSeconds;
      if (job->synthesis.audioSeconds > 0) {
        job->synthesis.realTimeFactor = job->synthesis.inferSeconds / job->synthesis.audioSeconds;
      }
      metrics.completedChunks++;
      markChunkFinished(*job);
    }
  } catch (const std::exception &e) {
    std::lock_guard<std::mutex> lock(job->mutex);
    const std::string message = e.what();
    if (message == "synthesis_cancelled") {
      job->cancelled = true;
    } else {
      job->failed = true;
      job->error = message;
      metrics.failedChunks++;
    }
    if (job->pendingChunks > job->inFlightChunks) {
      job->pendingChunks = job->inFlightChunks;
    }
    markChunkFinished(*job);
  }
}

void FairTtsScheduler::markChunkFinished(JobState &job) {
  if (job.inFlightChunks > 0) {
    --job.inFlightChunks;
  }
  if (job.pendingChunks > 0) {
    --job.pendingChunks;
  }
  if (job.pendingChunks == 0 && job.inFlightChunks == 0) {
    job.done = true;
    job.cv.notify_all();
  }
}

void FairTtsScheduler::assembleWav(const JobState &job) {
  std::uint64_t totalBytes = 0;
  for (const auto bytes : job.chunkBytes) {
    totalBytes += bytes;
  }

  if (totalBytes > std::numeric_limits<std::uint32_t>::max()) {
    throw std::runtime_error("Generated WAV exceeds 4 GiB. Use smaller inputs or raw output.");
  }

  std::ofstream output(job.outputPath, std::ios::binary);
  if (!output.good()) {
    throw std::runtime_error("Could not open output file");
  }

  writeServerWavHeader(job.sampleRate, job.sampleWidth, job.channels,
                       static_cast<std::uint32_t>(totalBytes), output);

  std::array<char, 64 * 1024> buffer{};
  for (const auto &chunkPath : job.chunkPaths) {
    std::ifstream chunk(chunkPath, std::ios::binary);
    if (!chunk.good()) {
      throw std::runtime_error("Missing synthesized chunk");
    }
    while (chunk.good()) {
      chunk.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
      const auto count = chunk.gcount();
      if (count > 0) {
        output.write(buffer.data(), count);
      }
    }
  }
}

void FairTtsScheduler::cleanupJob(JobState &job) {
  const auto allocated = job.allocatedTempBytes.exchange(0);
  if (allocated > 0) {
    const auto current = metrics.tempStorageBytes.load();
    if (current >= allocated) {
      metrics.tempStorageBytes.fetch_sub(allocated);
    } else {
      metrics.tempStorageBytes.store(0);
    }
  }

  std::error_code ignored;
  std::filesystem::remove_all(job.tempDir, ignored);
}

json resourcePolicyJson(const ServerOptions &options, const FairTtsScheduler &scheduler) {
return json{{"mode", "auto"},
            {"profile", options.cpuProfile},
            {"hardware_threads", options.resourcePolicy.hardwareThreads},
            {"memory_bytes", options.resourcePolicy.memoryBytes},
            {"cpu_threads_per_worker", options.cpuThreads.value_or(0)},
            {"max_concurrent_jobs", options.maxConcurrentJobs},
            {"chunk_workers", scheduler.workerTotal()},
            {"max_model_replicas", options.maxModelReplicas},
            {"queue_size", options.queueSize},
            {"queue_timeout_seconds", options.queueTimeoutSeconds},
            {"max_temp_bytes", options.maxTempBytes},
            {"active_jobs", scheduler.activeJobCount()},
            {"waiting_jobs", scheduler.waitingJobCount()}};
}

json metricsJson(const ServerMetrics &metrics, const FairTtsScheduler &scheduler,
               const ServerOptions &options) {
return json{{"jobs",
             json{{"active", scheduler.activeJobCount()},
                  {"waiting", scheduler.waitingJobCount()},
                  {"accepted", metrics.acceptedJobs.load()},
                  {"completed", metrics.completedJobs.load()},
                  {"cancelled", metrics.cancelledJobs.load()},
                  {"failed", metrics.failedJobs.load()},
                  {"rejected", metrics.rejectedJobs.load()}}},
            {"chunks",
             json{{"processing", metrics.processingChunks.load()},
                  {"completed", metrics.completedChunks.load()},
                  {"failed", metrics.failedChunks.load()}}},
            {"storage", json{{"temp_bytes", metrics.tempStorageBytes.load()},
                                {"max_temp_bytes", options.maxTempBytes},
                                {"output_retention_seconds", options.outputRetentionSeconds}}},
            {"resources", resourcePolicyJson(options, scheduler)},
            {"text_preprocessing",
             json{{"sanitized_inputs", metrics.sanitizedInputs.load()},
                  {"sanitize_warnings", metrics.sanitizeWarnings.load()},
                  {"rejected_inputs", metrics.rejectedTextInputs.load()}}}};
}

} // namespace piper_server
