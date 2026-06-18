#include "neo/package_reader.hpp"

#include <array>
#include <fstream>
#include <stdexcept>

#include "neo/binary_io.hpp"
#include "neo/compression.hpp"
#include "neo/constants.hpp"

namespace piper_neo::detail {

ParsedPackage parsePackage(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file.good()) {
    throw std::runtime_error("neo_not_found");
  }

  std::array<char, 8> magic{};
  file.read(magic.data(), static_cast<std::streamsize>(magic.size()));
  if (magic != NEO_MAGIC) {
    throw std::runtime_error("invalid_neo_magic");
  }

  ParsedPackage package;
  package.path = path;
  package.version = readU32(file);
  if (package.version != NEO_FORMAT_VERSION) {
    throw std::runtime_error("unsupported_neo_version");
  }

  const auto sectionCount = readU32(file);
  if (sectionCount == 0 || sectionCount > 128) {
    throw std::runtime_error("invalid_neo");
  }

  package.sections.reserve(sectionCount);
  for (std::uint32_t i = 0; i < sectionCount; ++i) {
    SectionEntry entry;
    entry.name = readString(file);
    entry.contentType = readString(file);
    entry.compression = readU32(file);
    entry.uncompressedSize = readU64(file);
    entry.storedSize = readU64(file);
    entry.offset = readU64(file);

    if (entry.name.empty()) {
      throw std::runtime_error("invalid_neo");
    }
    package.sections.push_back(entry);
  }

  return package;
}

const SectionEntry *findSection(const ParsedPackage &package, const std::string &name) {
  for (const auto &entry : package.sections) {
    if (entry.name == name) {
      return &entry;
    }
  }
  return nullptr;
}

std::vector<char> readSectionBytes(const ParsedPackage &package, const SectionEntry &entry) {
  std::ifstream file(package.path, std::ios::binary);
  if (!file.good()) {
    throw std::runtime_error("neo_not_found");
  }

  file.seekg(static_cast<std::streamoff>(entry.offset), std::ios::beg);
  std::vector<char> stored(static_cast<std::size_t>(entry.storedSize));
  if (!stored.empty()) {
    file.read(stored.data(), static_cast<std::streamsize>(stored.size()));
    if (!file.good()) {
      throw std::runtime_error("invalid_neo");
    }
  }

  return maybeDecompress(stored, entry);
}

std::string compressionName(std::uint32_t compression) {
  if (compression == NEO_COMPRESSION_NONE) {
    return "none";
  }
  if (compression == NEO_COMPRESSION_ZSTD) {
    return "zstd";
  }
  return "unknown";
}

} // namespace piper_neo::detail
