#ifndef PIPER_CORE_TEXT_CHUNKER_HPP_
#define PIPER_CORE_TEXT_CHUNKER_HPP_

#include <cstddef>
#include <string>
#include <vector>

namespace piper {

std::vector<std::string> splitTextIntoChunks(const std::string &text,
                                             std::size_t maxChunkBytes);

} // namespace piper

#endif // PIPER_CORE_TEXT_CHUNKER_HPP_
