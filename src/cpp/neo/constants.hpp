#ifndef PIPER_NEO_CONSTANTS_H_
#define PIPER_NEO_CONSTANTS_H_

#include <array>
#include <cstdint>

namespace piper_neo::detail {

inline constexpr std::array<char, 8> NEO_MAGIC{{'P', 'I', 'P', 'E', 'R', 'N', 'E', 'O'}};
inline constexpr std::uint32_t NEO_FORMAT_VERSION = 1;
inline constexpr std::uint32_t NEO_COMPRESSION_NONE = 0;
inline constexpr std::uint32_t NEO_COMPRESSION_ZSTD = 1;

} // namespace piper_neo::detail

#endif // PIPER_NEO_CONSTANTS_H_
