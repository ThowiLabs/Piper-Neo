#include "model_metadata.hpp"

#include <filesystem>
#include <optional>
#include <string>

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
      // Ignore invalid optional metadata. One bad field should not hide a model.
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

} // namespace piper_server
