#include "piper/api.hpp"

#include "core/synthesis_utils.hpp"
#include "core/wav/stream_chunks.hpp"
#include "core/wav/wav_header_writer.hpp"

#include <cstdint>
#include <functional>
#include <istream>
#include <ostream>
#include <string>

namespace piper {
namespace {

constexpr std::size_t DEFAULT_TEXT_CHUNK_BYTES = 4096;

std::size_t effectiveChunkBytes(std::size_t maxChunkBytes) {
  return maxChunkBytes == 0 ? DEFAULT_TEXT_CHUNK_BYTES : maxChunkBytes;
}

void beginWavStream(const SynthesisConfig &synthesisConfig,
                    std::ostream &audioFile,
                    std::streampos &headerPosition,
                    bool &isSeekable) {
  headerPosition = audioFile.tellp();
  isSeekable = (headerPosition != std::streampos(-1));
  core::writePlaceholderWavHeader(synthesisConfig, audioFile, isSeekable);
}

} // namespace

void textToWavFile(PiperConfig &config, Voice &voice, std::string text,
                   std::ostream &audioFile, SynthesisResult &result,
                   std::size_t maxChunkBytes,
                   const std::function<bool()> &shouldCancel) {
  maxChunkBytes = effectiveChunkBytes(maxChunkBytes);
  core::throwIfSynthesisCancelled(shouldCancel);

  const auto synthesisConfig = voice.synthesisConfig;
  std::streampos headerPosition{};
  bool isSeekable = false;
  beginWavStream(synthesisConfig, audioFile, headerPosition, isSeekable);

  std::uint64_t totalAudioBytes = 0;
  core::synthesizePendingText(config, voice, text, maxChunkBytes, audioFile,
                              result, totalAudioBytes, shouldCancel);

  core::patchSeekableWavHeader(synthesisConfig, audioFile, headerPosition,
                               totalAudioBytes);
}

void textToWavFileFromStream(PiperConfig &config, Voice &voice,
                             std::istream &textStream,
                             std::ostream &audioFile, SynthesisResult &result,
                             std::size_t maxChunkBytes,
                             const std::function<bool()> &shouldCancel) {
  maxChunkBytes = effectiveChunkBytes(maxChunkBytes);

  const auto synthesisConfig = voice.synthesisConfig;
  std::streampos headerPosition{};
  bool isSeekable = false;
  beginWavStream(synthesisConfig, audioFile, headerPosition, isSeekable);

  std::uint64_t totalAudioBytes = 0;
  std::string pending;
  std::string line;
  bool firstLine = true;

  while (std::getline(textStream, line)) {
    core::throwIfSynthesisCancelled(shouldCancel);
    if (firstLine) {
      firstLine = false;
      if (core::startsWithBytes(line, 0, {0xEF, 0xBB, 0xBF})) {
        line.erase(0, 3);
      }
    }

    pending.append(line);
    pending.push_back('\n');

    core::flushCompletedPendingChunks(config, voice, pending, maxChunkBytes,
                                      audioFile, result, totalAudioBytes,
                                      shouldCancel);
  }

  core::synthesizePendingText(config, voice, pending, maxChunkBytes, audioFile,
                              result, totalAudioBytes, shouldCancel);

  core::patchSeekableWavHeader(synthesisConfig, audioFile, headerPosition,
                               totalAudioBytes);
}

} // namespace piper
