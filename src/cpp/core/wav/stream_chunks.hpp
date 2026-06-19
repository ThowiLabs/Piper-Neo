#ifndef PIPER_CORE_WAV_STREAM_CHUNKS_HPP_
#define PIPER_CORE_WAV_STREAM_CHUNKS_HPP_

#include <cstdint>
#include <functional>
#include <iosfwd>
#include <string>

#include "piper/types.hpp"

namespace piper::core {

void synthesizeTextChunkToStream(PiperConfig &config, Voice &voice,
                                 const std::string &textChunk,
                                 std::ostream &audioFile,
                                 SynthesisResult &result,
                                 std::uint64_t &totalAudioBytes,
                                 const std::function<bool()> &shouldCancel);

void flushCompletedPendingChunks(PiperConfig &config, Voice &voice,
                                 std::string &pending,
                                 std::size_t maxChunkBytes,
                                 std::ostream &audioFile,
                                 SynthesisResult &result,
                                 std::uint64_t &totalAudioBytes,
                                 const std::function<bool()> &shouldCancel);

void synthesizePendingText(PiperConfig &config, Voice &voice,
                           const std::string &pending,
                           std::size_t maxChunkBytes,
                           std::ostream &audioFile,
                           SynthesisResult &result,
                           std::uint64_t &totalAudioBytes,
                           const std::function<bool()> &shouldCancel);

} // namespace piper::core

#endif // PIPER_CORE_WAV_STREAM_CHUNKS_HPP_
