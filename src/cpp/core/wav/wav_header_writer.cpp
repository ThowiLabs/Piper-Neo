#include "core/wav/wav_header_writer.hpp"

#include <limits>
#include <ostream>
#include <stdexcept>

#include <spdlog/spdlog.h>

#include "wavfile.hpp"

namespace piper::core {
namespace {

std::uint32_t placeholderDataSizeForStream(bool isSeekable) {
  if (isSeekable) {
    return 0;
  }

  return std::numeric_limits<std::uint32_t>::max() -
         static_cast<std::uint32_t>(sizeof(WavHeader)) + 8;
}

} // namespace

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

} // namespace piper::core
