#ifndef PIPER_CORE_SENTENCE_BOUNDARY_DETECTOR_HPP_
#define PIPER_CORE_SENTENCE_BOUNDARY_DETECTOR_HPP_

#include <cstddef>
#include <string>

namespace piper::core::sentence {

bool isUtf8ContinuationByte(char value);
bool isAsciiWhitespace(char value);
bool isExplicitSentenceBoundary(const std::string &text, std::size_t index,
                                std::size_t &splitAt);

} // namespace piper::core::sentence

#endif // PIPER_CORE_SENTENCE_BOUNDARY_DETECTOR_HPP_
