#include "piper.hpp"

#include "core/synthesis_utils.hpp"

#include <cstdint>
#include <functional>
#include <istream>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

#include "wavfile.hpp"

namespace piper {
namespace {

constexpr std::size_t DEFAULT_TEXT_CHUNK_BYTES = 4096;

std::uint32_t placeholderDataSizeForStream(bool isSeekable) {
  if (isSeekable) {
    return 0;
  }

  return std::numeric_limits<std::uint32_t>::max() -
         static_cast<std::uint32_t>(sizeof(WavHeader)) + 8;
}

void writePlaceholderWavHeader(const SynthesisConfig &synthesisConfig,
                               std::ostream &audioFile,
                               bool isSeekable) {
  writeWavHeaderBytes(synthesisConfig.sampleRate, synthesisConfig.sampleWidth,
                      synthesisConfig.channels,
                      placeholderDataSizeForStream(isSeekable), audioFile);
}

void patchSeekableWavHeader(const SynthesisConfig &synthesisConfig,
                            std::ostream &audioFile,
                            std::streampos headerPosition,
                            std::uint64_t totalAudioBytes) {
  const auto endPosition = audioFile.tellp();
  if ((headerPosition == std::streampos(-1)) ||
      (endPosition == std::streampos(-1))) {
    spdlog::warn("WAV stream is not seekable; wrote a best-effort streaming WAV "
                 "header. Use --output_raw for unbounded streaming output.");
    return;
  }

  const auto maxWavBytes =
      static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max());
  if (totalAudioBytes > maxWavBytes) {
    throw std::runtime_error(
        "Generated WAV exceeds 4 GiB. Use --output_raw for very large audio.");
  }

  audioFile.seekp(headerPosition);
  writeWavHeaderBytes(synthesisConfig.sampleRate, synthesisConfig.sampleWidth,
                      synthesisConfig.channels,
                      static_cast<std::uint32_t>(totalAudioBytes), audioFile);
  audioFile.seekp(endPosition);
}

void writeAudioBuffer(std::ostream &audioFile, std::vector<int16_t> &audioBuffer,
                      std::uint64_t &totalAudioBytes,
                      const std::function<bool()> &shouldCancel) {
  core::throwIfSynthesisCancelled(shouldCancel);
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

void synthesizeTextChunkToStream(PiperConfig &config, Voice &voice,
                                 const std::string &textChunk,
                                 std::ostream &audioFile,
                                 SynthesisResult &result,
                                 std::uint64_t &totalAudioBytes,
                                 const std::function<bool()> &shouldCancel) {
  core::throwIfSynthesisCancelled(shouldCancel);
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
    core::trimLeadingAsciiWhitespace(pending, leadingWhitespace);
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

} // namespace

void textToWavFile(PiperConfig &config, Voice &voice, std::string text,
                   std::ostream &audioFile, SynthesisResult &result,
                   std::size_t maxChunkBytes,
                   const std::function<bool()> &shouldCancel) {
  if (maxChunkBytes == 0) {
    maxChunkBytes = DEFAULT_TEXT_CHUNK_BYTES;
  }

  core::throwIfSynthesisCancelled(shouldCancel);
  auto chunks = splitTextIntoChunks(text, maxChunkBytes);

  const auto synthesisConfig = voice.synthesisConfig;
  const auto headerPosition = audioFile.tellp();
  const bool isSeekable = (headerPosition != std::streampos(-1));
  writePlaceholderWavHeader(synthesisConfig, audioFile, isSeekable);

  std::uint64_t totalAudioBytes = 0;
  for (const auto &textChunk : chunks) {
    synthesizeTextChunkToStream(config, voice, textChunk, audioFile, result,
                                totalAudioBytes, shouldCancel);
  }

  patchSeekableWavHeader(synthesisConfig, audioFile, headerPosition,
                         totalAudioBytes);
}

void textToWavFileFromStream(PiperConfig &config, Voice &voice,
                             std::istream &textStream,
                             std::ostream &audioFile, SynthesisResult &result,
                             std::size_t maxChunkBytes,
                             const std::function<bool()> &shouldCancel) {
  if (maxChunkBytes == 0) {
    maxChunkBytes = DEFAULT_TEXT_CHUNK_BYTES;
  }

  const auto synthesisConfig = voice.synthesisConfig;
  const auto headerPosition = audioFile.tellp();
  const bool isSeekable = (headerPosition != std::streampos(-1));
  writePlaceholderWavHeader(synthesisConfig, audioFile, isSeekable);

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

    flushCompletedPendingChunks(config, voice, pending, maxChunkBytes, audioFile,
                                result, totalAudioBytes, shouldCancel);
  }

  synthesizePendingText(config, voice, pending, maxChunkBytes, audioFile, result,
                        totalAudioBytes, shouldCancel);

  patchSeekableWavHeader(synthesisConfig, audioFile, headerPosition,
                         totalAudioBytes);
}

} // namespace piper
