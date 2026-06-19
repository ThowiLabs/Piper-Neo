#include "model_cache.hpp"

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>

#include "../neo_model.hpp"
#include "model_loader.hpp"
#include "model_paths.hpp"
#include "utils.hpp"

namespace piper_server {

ModelCache::ModelCache(piper::PiperConfig &piperConfig, piper::Voice &defaultVoice,
                       const ServerOptions &options, ModelRegistry &registry)
    : piperConfig(piperConfig), options(options), registry(registry),
      maxReplicas(std::max<std::size_t>(1, options.maxModelReplicas)) {
  const auto activeName = options.activeModelPath.filename().string();
  const auto activeConfig = options.activeModelConfigPath.empty()
                                ? std::filesystem::path(options.activeModelPath.string() + ".json")
                                : options.activeModelConfigPath;
  const bool activeIsNeo = piper_neo::isNeoFile(options.activeModelPath);
  auto runtime = std::make_shared<ModelRuntime>(
      ModelInfo{activeName, activeIsNeo ? "neo" : "onnx", options.activeModelPath, activeConfig,
                activeIsNeo || std::filesystem::exists(activeConfig), activeIsNeo});

  auto slot = std::make_unique<VoiceSlot>();
  slot->voice = &defaultVoice;
  runtime->slots.push_back(std::move(slot));

  cache[modelKey(options.activeModelPath)] = runtime;
}

VoiceLease ModelCache::checkout(const std::optional<std::string> &requestedModel) {
  auto info = resolve(requestedModel);
  auto runtime = runtimeFor(info);

  std::unique_lock<std::mutex> lock(runtime->mutex);
  while (true) {
    for (const auto &slot : runtime->slots) {
      if (!slot->inUse) {
        slot->inUse = true;
        return VoiceLease(runtime, slot.get());
      }
    }

    // Avoid a startup stampede: when several chunks request the same uncached
    // model, let only one thread load a replica. eSpeak/phonemize and Windows
    // file reads are process-global enough that concurrent initial loads can
    // trigger corrupted dictionary reads such as "es_dict length=0".
    if (runtime->loadingSlots > 0) {
      runtime->cv.wait(lock);
      continue;
    }

    if (runtime->slots.size() < maxReplicas) {
      break;
    }

    runtime->cv.wait(lock);
  }

  ++runtime->loadingSlots;
  lock.unlock();

  std::unique_ptr<piper::Voice> newVoice;
  try {
    newVoice = loadModelVoice(piperConfig, options, info);
  } catch (...) {
    lock.lock();
    if (runtime->loadingSlots > 0) {
      --runtime->loadingSlots;
    }
    runtime->cv.notify_all();
    lock.unlock();
    throw;
  }

  lock.lock();
  if (runtime->loadingSlots > 0) {
    --runtime->loadingSlots;
  }
  auto slot = std::make_unique<VoiceSlot>();
  slot->voice = newVoice.get();
  slot->ownedVoice = std::move(newVoice);
  slot->inUse = true;
  auto *slotPtr = slot.get();
  runtime->slots.push_back(std::move(slot));
  runtime->cv.notify_all();
  return VoiceLease(runtime, slotPtr);
}

ModelInfo ModelCache::resolve(const std::optional<std::string> &requestedModel) {
  const auto modelName = requestedModel ? trimCopy(*requestedModel) : std::string();
  auto info = findModelByName(options, modelName, &registry);
  if (!info) {
    throw std::runtime_error("model_not_found");
  }

  if (!info->hasConfig) {
    throw std::runtime_error("model_config_missing");
  }

  return *info;
}

std::shared_ptr<ModelRuntime> ModelCache::runtimeFor(const ModelInfo &info) {
  const auto key = modelKey(info.modelPath);
  std::lock_guard<std::mutex> lock(cacheMutex);
  auto it = cache.find(key);
  if (it != cache.end()) {
    return it->second;
  }

  auto runtime = std::make_shared<ModelRuntime>(info);
  cache[key] = runtime;
  return runtime;
}

} // namespace piper_server
