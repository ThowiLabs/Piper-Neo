#include "model_registry.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "../neo_model.hpp"
#include "model_metadata.hpp"
#include "model_scanner.hpp"
#include "utils.hpp"

namespace piper_server {

ModelRegistry::ModelRegistry(const ServerOptions &options)
    : options(options), refreshSeconds(std::max<std::size_t>(1, options.modelsRefreshSeconds)) {}

json ModelRegistry::list(const std::string &includeMode) {
  refreshIfNeeded();
  std::lock_guard<std::mutex> lock(mutex);
  json models = json::array();
  for (const auto &model : cachedModels) {
    models.push_back(modelInfoToJson(model, includeMode));
  }
  return models;
}

std::optional<ModelInfo> ModelRegistry::find(const std::string &modelName) {
  refreshIfNeeded();
  std::lock_guard<std::mutex> lock(mutex);
  for (const auto &model : cachedModels) {
    if (model.name == modelName) {
      return model;
    }
  }
  return std::nullopt;
}

void ModelRegistry::forceRefresh() {
  std::lock_guard<std::mutex> lock(mutex);
  refreshLocked();
}

void ModelRegistry::refreshIfNeeded() {
  const auto now = std::chrono::steady_clock::now();
  std::lock_guard<std::mutex> lock(mutex);
  if (!initialized || now >= nextRefresh) {
    refreshLocked();
  }
}

void ModelRegistry::refreshLocked() {
  cachedModels = scanModels(options.modelsDir);
  initialized = true;
  nextRefresh = std::chrono::steady_clock::now() + std::chrono::seconds(refreshSeconds);
}

std::optional<ModelInfo> findModelByName(const ServerOptions &options,
                                         const std::string &requestedModel,
                                         ModelRegistry *registry) {
  std::string modelName = trimCopy(requestedModel);
  if (modelName.empty()) {
    modelName = options.activeModelPath.filename().string();
  }

  if (!isSafeFileName(modelName)) {
    throw std::runtime_error("invalid_model");
  }

  std::vector<std::string> candidates{modelName};
  if (std::filesystem::path(modelName).extension().empty()) {
    candidates.push_back(modelName + ".onnx");
    candidates.push_back(modelName + ".neo");
  }

  for (const auto &candidate : candidates) {
    if (candidate == options.activeModelPath.filename().string()) {
      const auto configPath = options.activeModelConfigPath.empty()
                                  ? std::filesystem::path(options.activeModelPath.string() + ".json")
                                  : options.activeModelConfigPath;
      const bool activeIsNeo = piper_neo::isNeoFile(options.activeModelPath);
      return ModelInfo{candidate, activeIsNeo ? "neo" : "onnx", options.activeModelPath,
                       configPath, activeIsNeo || std::filesystem::exists(configPath),
                       activeIsNeo};
    }
  }

  for (const auto &candidate : candidates) {
    if (registry != nullptr) {
      if (auto model = registry->find(candidate)) {
        return model;
      }
      continue;
    }

    for (const auto &model : scanModels(options.modelsDir)) {
      if (model.name == candidate) {
        return model;
      }
    }
  }

  return std::nullopt;
}

} // namespace piper_server
