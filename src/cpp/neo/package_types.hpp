#ifndef PIPER_NEO_PACKAGE_TYPES_H_
#define PIPER_NEO_PACKAGE_TYPES_H_

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "neo/constants.hpp"

namespace piper_neo::detail {

struct SectionEntry {
  std::string name;
  std::string contentType;
  std::uint32_t compression = NEO_COMPRESSION_NONE;
  std::uint64_t uncompressedSize = 0;
  std::uint64_t storedSize = 0;
  std::uint64_t offset = 0;
};

struct ParsedPackage {
  std::filesystem::path path;
  std::uint32_t version = NEO_FORMAT_VERSION;
  std::vector<SectionEntry> sections;
};

} // namespace piper_neo::detail

#endif // PIPER_NEO_PACKAGE_TYPES_H_
