#ifndef PIPER_CORE_WAV_HEADER_WRITER_HPP_
#define PIPER_CORE_WAV_HEADER_WRITER_HPP_

#include <cstdint>
#include <iosfwd>

#include "piper/types.hpp"

namespace piper::core {

void writePlaceholderWavHeader(const SynthesisConfig &synthesisConfig,
                               std::ostream &audioFile,
                               bool isSeekable);

void patchSeekableWavHeader(const SynthesisConfig &synthesisConfig,
                            std::ostream &audioFile,
                            std::streampos headerPosition,
                            std::uint64_t totalAudioBytes);

} // namespace piper::core

#endif // PIPER_CORE_WAV_HEADER_WRITER_HPP_
