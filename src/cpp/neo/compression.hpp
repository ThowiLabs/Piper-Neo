#ifndef PIPER_NEO_COMPRESSION_H_
#define PIPER_NEO_COMPRESSION_H_

#include <cstdint>
#include <vector>

#include "neo/package_types.hpp"

namespace piper_neo::detail {

std::vector<char> compressZstd(const std::vector<char> &input, int level);
std::vector<char> decompressZstd(const std::vector<char> &input, std::uint64_t expectedSize);
std::vector<char> maybeDecompress(const std::vector<char> &stored, const SectionEntry &entry);

} // namespace piper_neo::detail

#endif // PIPER_NEO_COMPRESSION_H_
