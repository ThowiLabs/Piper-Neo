#include "server/jobs/chunk_worker.hpp"

#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "piper.hpp"
#include "server/jobs/job_lifecycle.hpp"

namespace piper_server {

namespace {

class SynthesisConfigGuard {
public:
  explicit SynthesisConfigGuard(piper::Voice &voice)
      : voice(voice), speakerId(voice.synthesisConfig.speakerId),
        noiseScale(voice.synthesisConfig.noiseScale),
        lengthScale(voice.synthesisConfig.lengthScale),
        noiseW(voice.synthesisConfig.noiseW),
        sentenceSilenceSeconds(voice.synthesisConfig.sentenceSilenceSeconds) {}

  ~SynthesisConfigGuard() {
    voice.synthesisConfig.speakerId = speakerId;
    voice.synthesisConfig.noiseScale = noiseScale;
    voice.synthesisConfig.lengthScale = lengthScale;
    voice.synthesisConfig.noiseW = noiseW;
    voice.synthesisConfig.sentenceSilenceSeconds = sentenceSilenceSeconds;
  }

private:
  piper::Voice &voice;
  std::optional<piper::SpeakerId> speakerId;
  float noiseScale = 0.0f;
  float lengthScale = 0.0f;
  float noiseW = 0.0f;
  float sentenceSilenceSeconds = 0.0f;
};

void applyJobOverrides(piper::Voice &voice, const TtsJobState &job) {
  if (job.speakerId) {
    voice.synthesisConfig.speakerId = *job.speakerId;
  }
  if (job.noiseScale) {
    voice.synthesisConfig.noiseScale = *job.noiseScale;
  }
  if (job.lengthScale) {
    voice.synthesisConfig.lengthScale = *job.lengthScale;
  }
  if (job.noiseW) {
    voice.synthesisConfig.noiseW = *job.noiseW;
  }
  if (job.sentenceSilenceSeconds) {
    voice.synthesisConfig.sentenceSilenceSeconds = *job.sentenceSilenceSeconds;
  }
}

} // namespace

void synthesizeJobChunk(piper::PiperConfig &piperConfig, ModelCache &modelCache,
                        const ServerOptions &options, ServerMetrics &metrics,
                        const std::shared_ptr<TtsJobState> &job,
                        std::size_t index) {
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
    SynthesisConfigGuard configGuard(selectedVoice);
    applyJobOverrides(selectedVoice, *job);

    const auto chunkPath = job->tempDir / ("chunk_" + std::to_string(index) + ".raw");
    if (shouldCancel()) {
      throw std::runtime_error("synthesis_cancelled");
    }

    std::ofstream chunkFile(chunkPath, std::ios::binary);
    if (!chunkFile.good()) {
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

    piper::textToAudio(piperConfig, selectedVoice, job->textChunks[index], audioBuffer,
                       chunkResult, audioCallback, shouldCancel);
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

} // namespace piper_server
