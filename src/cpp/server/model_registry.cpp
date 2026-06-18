#include "model_registry.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "../neo_model.hpp"
#include "utils.hpp"

namespace piper_server {
namespace {

template <typename T>
void copyIfExists(json &to, const json &from, const std::string &key) {
  if (from.contains(key) && !from[key].is_null()) {
    try {
      to[key] = from[key].get<T>();
    } catch (const json::exception &) {
      // Ignore invalid metadata fields. A bad optional field should not hide a model.
    }
  }
}

} // namespace

bool modelJsonHasImage(const json &root) {
  return root.contains("modelcard") && root["modelcard"].is_object() &&
         root["modelcard"].contains("image") && root["modelcard"]["image"].is_string() &&
         !root["modelcard"]["image"].get<std::string>().empty();
}

json modelInfoToJson(const ModelInfo &modelInfo, const std::string &includeMode) {
  json out{{"file", modelInfo.name},
           {"name", modelInfo.name},
           {"format", modelInfo.format},
           {"config_file", modelInfo.isNeo ? "embedded" : modelInfo.configPath.filename().string()},
           {"available", std::filesystem::exists(modelInfo.modelPath)},
           {"has_config", modelInfo.hasConfig},
           {"config_valid", false}};

  if (!modelInfo.hasConfig) {
    return out;
  }

  json root;
  std::optional<piper_neo::NeoPackageInfo> neoInfo;
  if (modelInfo.isNeo) {
    try {
      neoInfo = piper_neo::inspectPackage(modelInfo.modelPath);
      root = neoInfo->metadata;
    } catch (const std::exception &e) {
      out["config_error"] = e.what();
      return out;
    }
  } else {
    std::string configError;
    auto maybeRoot = tryLoadJsonFile(modelInfo.configPath, configError);
    if (!maybeRoot) {
      out["config_error"] = configError.empty() ? "invalid_json" : configError;
      return out;
    }
    root = *maybeRoot;
  }

  out["config_valid"] = true;
  if (neoInfo) {
    out["neo"] = json{{"version", neoInfo->version},
                       {"model_compression", neoInfo->modelCompression},
                       {"model_bytes", neoInfo->modelBytes},
                       {"stored_model_bytes", neoInfo->storedModelBytes}};
  }

  json modelcard = json::object();
  if (root.contains("modelcard") && root["modelcard"].is_object()) {
    const auto &card = root["modelcard"];
    copyIfExists<std::string>(modelcard, card, "id");
    copyIfExists<std::string>(modelcard, card, "name");
    copyIfExists<std::string>(modelcard, card, "description");
    copyIfExists<std::string>(modelcard, card, "language");
    copyIfExists<std::string>(modelcard, card, "voiceprompt");
    copyIfExists<std::string>(modelcard, card, "sha256");
  }

  const bool hasImage = neoInfo ? neoInfo->hasImage : modelJsonHasImage(root);
  out["has_image"] = hasImage;
  if (hasImage) {
    out["image_url"] = "/api/v1/models/" + modelInfo.name + "/image";
  }

  if (!modelcard.empty()) {
    out["modelcard"] = modelcard;
    if (modelcard.contains("name")) {
      out["name"] = modelcard["name"];
    }
    if (modelcard.contains("language")) {
      out["language"] = modelcard["language"];
    }
  }

  if (includeMode == "basic") {
    return out;
  }

  if (root.contains("dataset")) {
    out["dataset"] = root["dataset"];
  }
  if (root.contains("audio") && root["audio"].is_object()) {
    json audio = json::object();
    copyIfExists<int>(audio, root["audio"], "sample_rate");
    copyIfExists<std::string>(audio, root["audio"], "quality");
    out["audio"] = audio;
  }
  if (root.contains("language") && root["language"].is_object()) {
    json language = json::object();
    copyIfExists<std::string>(language, root["language"], "code");
    out["language_info"] = language;
  }
  if (root.contains("espeak") && root["espeak"].is_object()) {
    json espeak = json::object();
    copyIfExists<std::string>(espeak, root["espeak"], "voice");
    out["espeak"] = espeak;
  }
  if (root.contains("inference") && root["inference"].is_object()) {
    json inference = json::object();
    copyIfExists<double>(inference, root["inference"], "noise_scale");
    copyIfExists<double>(inference, root["inference"], "length_scale");
    copyIfExists<double>(inference, root["inference"], "noise_w");
    out["inference"] = inference;
  }
  copyIfExists<int>(out, root, "num_speakers");
  copyIfExists<std::string>(out, root, "piper_version");

  if (includeMode == "technical") {
    copyIfExists<std::string>(out, root, "phoneme_type");
    copyIfExists<int>(out, root, "num_symbols");
    if (root.contains("speaker_id_map") && root["speaker_id_map"].is_object()) {
      out["speaker_id_map"] = root["speaker_id_map"];
    }
  }

  return out;
}


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

std::string modelKey(const std::filesystem::path &modelPath) {
  std::error_code ignored;
  auto absolutePath = std::filesystem::absolute(modelPath, ignored);
  if (ignored) {
    return modelPath.lexically_normal().string();
  }

  auto canonicalPath = std::filesystem::weakly_canonical(absolutePath, ignored);
  if (ignored) {
    return absolutePath.lexically_normal().string();
  }

  return canonicalPath.string();
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
      return ModelInfo{candidate, piper_neo::isNeoFile(options.activeModelPath) ? "neo" : "onnx",
                       options.activeModelPath, configPath,
                       piper_neo::isNeoFile(options.activeModelPath) || std::filesystem::exists(configPath),
                       piper_neo::isNeoFile(options.activeModelPath)};
    }
  }

  for (const auto &candidate : candidates) {
    if (registry != nullptr) {
      if (auto model = registry->find(candidate)) {
        return model;
      }
      continue;
    }

    const auto models = scanModels(options.modelsDir);
    for (const auto &model : models) {
      if (model.name == candidate) {
        return model;
      }
    }
  }

  return std::nullopt;
}

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
