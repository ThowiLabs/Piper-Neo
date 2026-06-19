#include "model_scanner.hpp"

#include <algorithm>
#include <filesystem>
#include <optional>
#include <vector>

#include "../neo_model.hpp"
#include "utils.hpp"

namespace piper_server {

std::vector<ModelInfo> scanModels(const std::filesystem::path &modelsDir) {
  std::vector<ModelInfo> models;
  if (!std::filesystem::exists(modelsDir)) {
    return models;
  }

  for (const auto &entry : std::filesystem::directory_iterator(modelsDir)) {
    if (!entry.is_regular_file()) {
      continue;
    }

    auto path = entry.path();
    const auto ext = lowerCopy(path.extension().string());
    if (ext == ".onnx") {
      auto configPath = std::filesystem::path(path.string() + ".json");
      models.push_back(ModelInfo{path.filename().string(), "onnx", path, configPath,
                                 std::filesystem::exists(configPath), false});
    } else if (ext == ".neo") {
      models.push_back(ModelInfo{path.filename().string(), "neo", path, {}, true, true});
    }
  }

  std::sort(models.begin(), models.end(), [](const ModelInfo &a, const ModelInfo &b) {
    return a.name < b.name;
  });

  return models;
}

std::optional<ModelInfo> findFirstUsableModel(const std::filesystem::path &modelsDir) {
  for (const auto &model : scanModels(modelsDir)) {
    if (model.hasConfig) {
      return model;
    }
  }

  return std::nullopt;
}

} // namespace piper_server
