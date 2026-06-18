#include "neo_model.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>

#include "neo/file_utils.hpp"
#include "neo/package_reader.hpp"
#include "neo/package_writer.hpp"

namespace piper_neo {

bool isNeoFile(const std::filesystem::path &path) {
  return detail::lowerCopy(path.extension().string()) == ".neo";
}

NeoPackageInfo inspectPackage(const std::filesystem::path &neoPath) {
  auto package = detail::parsePackage(neoPath);
  auto metadataEntry = detail::findSection(package, "metadata.json");
  auto modelEntry = detail::findSection(package, "model.onnx");
  if (metadataEntry == nullptr || modelEntry == nullptr) {
    throw std::runtime_error("invalid_neo");
  }

  const auto metadataBytes = detail::readSectionBytes(package, *metadataEntry);
  const std::string metadataText(metadataBytes.begin(), metadataBytes.end());

  NeoPackageInfo info;
  info.path = neoPath;
  info.version = package.version;
  info.metadata = nlohmann::json::parse(metadataText);
  info.modelBytes = modelEntry->uncompressedSize;
  info.storedModelBytes = modelEntry->storedSize;
  info.modelCompression = detail::compressionName(modelEntry->compression);

  if (auto image = detail::findSection(package, "image")) {
    info.hasImage = true;
    info.imageContentType = image->contentType;
  }

  return info;
}

ExtractedNeoModel extractPackage(const std::filesystem::path &neoPath,
                                 const std::filesystem::path &cacheRoot) {
  auto package = detail::parsePackage(neoPath);
  auto metadataEntry = detail::findSection(package, "metadata.json");
  auto modelEntry = detail::findSection(package, "model.onnx");
  if (metadataEntry == nullptr || modelEntry == nullptr) {
    throw std::runtime_error("invalid_neo");
  }

  auto info = inspectPackage(neoPath);
  auto cacheKey = detail::fnv1aHex(std::filesystem::absolute(neoPath).string());
  auto outDir = cacheRoot / cacheKey;
  auto outModel = outDir / (neoPath.stem().string() + ".onnx");
  auto outConfig = std::filesystem::path(outModel.string() + ".json");

  std::filesystem::create_directories(outDir);
  if (!std::filesystem::exists(outModel) || !std::filesystem::exists(outConfig)) {
    auto modelBytes = detail::readSectionBytes(package, *modelEntry);
    auto metadataBytes = detail::readSectionBytes(package, *metadataEntry);
    detail::writeFileBytes(outModel, modelBytes);
    detail::writeFileBytes(outConfig, metadataBytes);
  }

  ExtractedNeoModel extracted;
  extracted.directory = outDir;
  extracted.modelPath = outModel;
  extracted.configPath = outConfig;
  extracted.info = std::move(info);
  return extracted;
}

std::pair<std::string, std::string> readImageSection(const std::filesystem::path &neoPath) {
  auto package = detail::parsePackage(neoPath);
  auto imageEntry = detail::findSection(package, "image");
  if (imageEntry == nullptr) {
    throw std::runtime_error("not_found");
  }

  auto imageBytes = detail::readSectionBytes(package, *imageEntry);
  return {imageEntry->contentType, std::string(imageBytes.begin(), imageBytes.end())};
}

void writePackageFromOnnx(const std::filesystem::path &onnxPath,
                          const std::filesystem::path &configPath,
                          const std::filesystem::path &outputNeoPath,
                          const std::optional<std::filesystem::path> &imagePath,
                          int compressionLevel) {
  detail::writePackageFromOnnxImpl(onnxPath, configPath, outputNeoPath, imagePath, compressionLevel);
}

} // namespace piper_neo
