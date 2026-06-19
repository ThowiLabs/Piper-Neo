#include "server/jobs/job_lifecycle.hpp"

#include <filesystem>

#include "piper.hpp"
#include "server/jobs/chunked_wav.hpp"
#include "server/utils.hpp"

namespace piper_server {

std::shared_ptr<TtsJobState> createJobState(const TtsJobRequest &request,
                                            const ServerOptions &options) {
  auto job = std::make_shared<TtsJobState>();
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
  return job;
}

void markChunkFinished(TtsJobState &job) {
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

void assembleJobWav(const TtsJobState &job) {
  ChunkedWavOutput output;
  output.outputPath = job.outputPath;
  output.chunkPaths = job.chunkPaths;
  output.chunkBytes = job.chunkBytes;
  output.sampleRate = job.sampleRate;
  output.sampleWidth = job.sampleWidth;
  output.channels = job.channels;
  assembleChunkedWav(output);
}

void cleanupJobTemp(TtsJobState &job, ServerMetrics &metrics) {
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

TtsJobResult makeJobResult(const TtsJobState &job) {
  TtsJobResult result;
  result.fileName = job.fileName;
  result.outputPath = job.outputPath;
  result.modelName = job.modelName;
  result.modelPath = job.modelPath;
  result.chunks = job.textChunks.size();
  result.synthesis = job.synthesis;
  if (std::filesystem::exists(job.outputPath)) {
    result.bytes = std::filesystem::file_size(job.outputPath);
  }
  return result;
}

} // namespace piper_server
