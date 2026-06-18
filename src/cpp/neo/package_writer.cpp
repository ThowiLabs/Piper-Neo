#include "neo/package_writer.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "json.hpp"
#include "neo/binary_io.hpp"
#include "neo/compression.hpp"
#include "neo/constants.hpp"
#include "neo/file_utils.hpp"
#include "neo/image_payload.hpp"
#include "neo/package_types.hpp"

namespace piper_neo::detail {
namespace {

void addSection(std::vector<SectionEntry> &entries, std::vector<std::vector<char>> &payloads,
                const std::string &name, const std::string &contentType,
                const std::vector<char> &raw, bool useZstd, int compressionLevel) {
  SectionEntry entry;
  entry.name = name;
  entry.contentType = contentType;
  entry.uncompressedSize = raw.size();

  if (useZstd) {
    entry.compression = NEO_COMPRESSION_ZSTD;
    payloads.push_back(compressZstd(raw, compressionLevel));
  } else {
    entry.compression = NEO_COMPRESSION_NONE;
    payloads.push_back(raw);
  }

  entry.storedSize = payloads.back().size();
  entries.push_back(entry);
}

std::uint64_t calculateDirectorySize(const std::vector<SectionEntry> &entries) {
  std::uint64_t directorySize = 8 + 4 + 4;
  for (const auto &entry : entries) {
    directorySize += 4 + entry.name.size();
    directorySize += 4 + entry.contentType.size();
    directorySize += 4 + 8 + 8 + 8;
  }
  return directorySize;
}

void writeDirectory(std::ostream &out, const std::vector<SectionEntry> &entries) {
  out.write(NEO_MAGIC.data(), static_cast<std::streamsize>(NEO_MAGIC.size()));
  writeU32(out, NEO_FORMAT_VERSION);
  writeU32(out, static_cast<std::uint32_t>(entries.size()));

  for (const auto &entry : entries) {
    writeString(out, entry.name);
    writeString(out, entry.contentType);
    writeU32(out, entry.compression);
    writeU64(out, entry.uncompressedSize);
    writeU64(out, entry.storedSize);
    writeU64(out, entry.offset);
  }
}

void writePayloads(std::ostream &out, const std::vector<std::vector<char>> &payloads) {
  for (const auto &payload : payloads) {
    if (!payload.empty()) {
      out.write(payload.data(), static_cast<std::streamsize>(payload.size()));
    }
  }
}

} // namespace

void writePackageFromOnnxImpl(const std::filesystem::path &onnxPath,
                              const std::filesystem::path &configPath,
                              const std::filesystem::path &outputNeoPath,
                              const std::optional<std::filesystem::path> &imagePath,
                              int compressionLevel) {
  auto modelBytes = readFileBytes(onnxPath);
  auto configBytes = readFileBytes(configPath);
  nlohmann::json metadata = nlohmann::json::parse(std::string(configBytes.begin(), configBytes.end()));

  std::optional<std::vector<char>> imageBytes;
  std::string imageContentType;
  if (imagePath) {
    imageBytes = readFileBytes(*imagePath);
    imageContentType = contentTypeForImagePath(*imagePath);
  } else if (metadata.contains("modelcard") && metadata["modelcard"].is_object() &&
             metadata["modelcard"].contains("image") && metadata["modelcard"]["image"].is_string()) {
    auto decoded = decodeDataImage(metadata["modelcard"]["image"].get<std::string>());
    imageContentType = decoded.first;
    imageBytes = std::move(decoded.second);
  }

  if (metadata.contains("modelcard") && metadata["modelcard"].is_object()) {
    metadata["modelcard"].erase("image");
  }

  metadata["piper_neo"] = nlohmann::json{{"format", "piper-neo"},
                                           {"format_version", NEO_FORMAT_VERSION},
                                           {"model_section", "model.onnx"},
                                           {"compression", "zstd"}};

  const auto metadataText = metadata.dump(2);
  std::vector<char> metadataPayload(metadataText.begin(), metadataText.end());

  std::vector<SectionEntry> entries;
  std::vector<std::vector<char>> payloads;
  addSection(entries, payloads, "metadata.json", "application/json", metadataPayload, true, compressionLevel);
  addSection(entries, payloads, "model.onnx", "application/onnx", modelBytes, true, compressionLevel);
  if (imageBytes) {
    addSection(entries, payloads, "image", imageContentType, *imageBytes, true, compressionLevel);
  }

  std::uint64_t offset = calculateDirectorySize(entries);
  for (std::size_t i = 0; i < entries.size(); ++i) {
    entries[i].offset = offset;
    offset += entries[i].storedSize;
  }

  std::filesystem::create_directories(outputNeoPath.parent_path().empty() ? std::filesystem::path(".") : outputNeoPath.parent_path());
  std::ofstream out(outputNeoPath, std::ios::binary);
  if (!out.good()) {
    throw std::runtime_error("file_write_error");
  }

  writeDirectory(out, entries);
  writePayloads(out, payloads);
}

} // namespace piper_neo::detail
