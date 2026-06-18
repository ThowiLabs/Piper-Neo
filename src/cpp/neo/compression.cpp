#include "neo/compression.hpp"

#include <stdexcept>
#include <string>

#ifdef PIPER_NEO_HAS_ZSTD
#include <zstd.h>
#endif

namespace piper_neo::detail {

std::vector<char> compressZstd(const std::vector<char> &input, int level) {
#ifdef PIPER_NEO_HAS_ZSTD
  if (input.empty()) {
    return {};
  }
  if (level <= 0) {
    level = 10;
  }

  const auto bound = ZSTD_compressBound(input.size());
  std::vector<char> output(bound);
  const auto size = ZSTD_compress(output.data(), output.size(), input.data(), input.size(), level);
  if (ZSTD_isError(size)) {
    throw std::runtime_error(std::string("zstd_compress_error: ") + ZSTD_getErrorName(size));
  }

  output.resize(size);
  return output;
#else
  (void)input;
  (void)level;
  throw std::runtime_error("zstd_not_available");
#endif
}

std::vector<char> decompressZstd(const std::vector<char> &input, std::uint64_t expectedSize) {
#ifdef PIPER_NEO_HAS_ZSTD
  std::vector<char> output(static_cast<std::size_t>(expectedSize));
  const auto size = ZSTD_decompress(output.data(), output.size(), input.data(), input.size());
  if (ZSTD_isError(size)) {
    throw std::runtime_error(std::string("zstd_decompress_error: ") + ZSTD_getErrorName(size));
  }
  if (size != expectedSize) {
    throw std::runtime_error("invalid_neo_size");
  }
  return output;
#else
  (void)input;
  (void)expectedSize;
  throw std::runtime_error("zstd_not_available");
#endif
}

std::vector<char> maybeDecompress(const std::vector<char> &stored, const SectionEntry &entry) {
  if (entry.compression == NEO_COMPRESSION_NONE) {
    if (stored.size() != entry.uncompressedSize) {
      throw std::runtime_error("invalid_neo_size");
    }
    return stored;
  }

  if (entry.compression == NEO_COMPRESSION_ZSTD) {
    return decompressZstd(stored, entry.uncompressedSize);
  }

  throw std::runtime_error("unsupported_neo_compression");
}

} // namespace piper_neo::detail
