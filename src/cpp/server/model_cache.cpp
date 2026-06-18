#include "model_cache.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include <spdlog/spdlog.h>

#include "../neo_model.hpp"
#include "utils.hpp"

namespace piper_server {
namespace {

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

} // namespace

ModelRuntime::ModelRuntime(ModelInfo modelInfo) : info(std::move(modelInfo)) {}

VoiceLease::VoiceLease(std::shared_ptr<ModelRuntime> modelRuntime, VoiceSlot *voiceSlot)
    : runtime(std::move(modelRuntime)), slot(voiceSlot) {}

VoiceLease::VoiceLease(VoiceLease &&other) noexcept
    : runtime(std::move(other.runtime)), slot(other.slot) {
  other.slot = nullptr;
}

VoiceLease &VoiceLease::operator=(VoiceLease &&other) noexcept {
  if (this != &other) {
    reset();
    runtime = std::move(other.runtime);
    slot = other.slot;
    other.slot = nullptr;
  }
  return *this;
}

VoiceLease::~VoiceLease() { reset(); }

piper::Voice &VoiceLease::get() const { return *slot->voice; }
const ModelInfo &VoiceLease::model() const { return runtime->info; }
VoiceLease::operator bool() const { return slot != nullptr && slot->voice != nullptr; }

void VoiceLease::reset() {
  if (runtime && slot != nullptr) {
    std::lock_guard<std::mutex> lock(runtime->mutex);
    slot->inUse = false;
    runtime->cv.notify_one();
  }
  slot = nullptr;
  runtime.reset();
}

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
  for (const auto &slot : runtime->slots) {
    if (!slot->inUse) {
      slot->inUse = true;
      return VoiceLease(runtime, slot.get());
    }
  }

  while ((runtime->slots.size() + runtime->loadingSlots) >= maxReplicas) {
    runtime->cv.wait(lock, [&runtime]() {
      for (const auto &slot : runtime->slots) {
        if (!slot->inUse) {
          return true;
        }
      }
      return false;
    });

    for (const auto &slot : runtime->slots) {
      if (!slot->inUse) {
        slot->inUse = true;
        return VoiceLease(runtime, slot.get());
      }
    }
  }

  ++runtime->loadingSlots;
  auto newVoice = std::make_unique<piper::Voice>();
  auto speakerId = options.defaultSpeakerId;
  lock.unlock();

  try {
    const auto loadStart = std::chrono::steady_clock::now();
    spdlog::info("{} Model load started: model={} format={}", nowIso8601(), info.name, info.format);
    auto loadModelPath = info.modelPath;
    auto loadConfigPath = info.configPath;
    if (info.isNeo) {
      auto extracted = piper_neo::extractPackage(info.modelPath, options.outputDir / "neo-cache");
      loadModelPath = extracted.modelPath;
      loadConfigPath = extracted.configPath;
    }
    piper::loadVoice(piperConfig, loadModelPath.string(), loadConfigPath.string(),
                     *newVoice, speakerId, options.useCuda, options.cpuThreads);
    const auto loadEnd = std::chrono::steady_clock::now();
    spdlog::info("{} Model load finished: model={} format={} duration_ms={}", nowIso8601(),
                 info.name, info.format,
                 std::chrono::duration_cast<std::chrono::milliseconds>(loadEnd - loadStart).count());
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
