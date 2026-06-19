#include "core/wav/stream_chunks.hpp"

#include <ostream>
#include <stdexcept>
#include <vector>

#include "core/synthesis_utils.hpp"
#include "piper/api.hpp"

namespace piper::core {
namespace {

void writeAudioBuffer(std::ostream &audioFile, std::vector<int16_t> &audioBuffer,
                      std::uint64_t &totalAudioBytes,
                      const std::function<bool()> &shouldCancel) {
  throwIfSynthesisCancelled(shouldCancel);
  if (audioBuffer.empty()) {
    return;
  }

  const std::size_t audioBytes = sizeof(int16_t) * audioBuffer.size();
  audioFile.write(reinterpret_cast<const char *>(audioBuffer.data()), audioBytes);
  if (!audioFile.good()) {
    throw std::runtime_error("Failed to write WAV audio data");
  }

  totalAudioBytes += audioBytes;
}

} // namespace

void synthesizeTextChunkToStream(PiperConfig &config, Voice &voice,
                                 const std::string &textChunk,
                                 std::ostream &audioFile,
                                 SynthesisResult &result,
                                 std::uint64_t &totalAudioBytes,
                                 const std::function<bool()> &shouldCancel) {
  throwIfSynthesisCancelled(shouldCancel);
  if (textChunk.empty()) {
    return;
  }

  std::vector<int16_t> audioBuffer;
  auto audioCallback = [&audioFile, &audioBuffer, &totalAudioBytes,
                        &shouldCancel]() {
    writeAudioBuffer(audioFile, audioBuffer, totalAudioBytes, shouldCancel);
  };

  textToAudio(config, voice, textChunk, audioBuffer, result, audioCallback,
              shouldCancel);
}

void flushCompletedPendingChunks(PiperConfig &config, Voice &voice,
                                 std::string &pending,
                                 std::size_t maxChunkBytes,
                                 std::ostream &audioFile,
                                 SynthesisResult &result,
                                 std::uint64_t &totalAudioBytes,
                                 const std::function<bool()> &shouldCancel) {
  while (pending.size() > (maxChunkBytes * 2)) {
    auto chunks = splitTextIntoChunks(pending, maxChunkBytes);
    if (chunks.empty()) {
      break;
    }

    const std::size_t chunksToSynthesize = chunks.size() > 1 ? chunks.size() - 1 : 1;
    std::size_t consumedBytes = 0;

    for (std::size_t i = 0; i < chunksToSynthesize; ++i) {
      synthesizeTextChunkToStream(config, voice, chunks[i], audioFile, result,
                                  totalAudioBytes, shouldCancel);
      consumedBytes += chunks[i].size();
    }

    if ((consumedBytes == 0) || (consumedBytes >= pending.size())) {
      pending.clear();
      break;
    }

    pending.erase(0, consumedBytes);
    std::size_t leadingWhitespace = 0;
    trimLeadingAsciiWhitespace(pending, leadingWhitespace);
    if (leadingWhitespace > 0) {
      pending.erase(0, leadingWhitespace);
    }
  }
}

void synthesizePendingText(PiperConfig &config, Voice &voice,
                           const std::string &pending,
                           std::size_t maxChunkBytes,
                           std::ostream &audioFile,
                           SynthesisResult &result,
                           std::uint64_t &totalAudioBytes,
                           const std::function<bool()> &shouldCancel) {
  if (pending.empty()) {
    return;
  }

  auto chunks = splitTextIntoChunks(pending, maxChunkBytes);
  for (const auto &textChunk : chunks) {
    synthesizeTextChunkToStream(config, voice, textChunk, audioFile, result,
                                totalAudioBytes, shouldCancel);
  }
}

} // namespace piper::core
