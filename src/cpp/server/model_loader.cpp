#include "server/model_loader.hpp"

#include <chrono>
#include <memory>

#include <spdlog/spdlog.h>

#include "neo_model.hpp"
#include "piper.hpp"
#include "server/utils.hpp"

namespace piper_server {

std::unique_ptr<piper::Voice> loadModelVoice(piper::PiperConfig &piperConfig,
                                             const ServerOptions &options,
                                             const ModelInfo &info) {
  auto voice = std::make_unique<piper::Voice>();
  auto speakerId = options.defaultSpeakerId;
  auto loadModelPath = info.modelPath;
  auto loadConfigPath = info.configPath;

  const auto loadStart = std::chrono::steady_clock::now();
  spdlog::info("{} Model load started: model={} format={}", nowIso8601(), info.name, info.format);

  if (info.isNeo) {
    auto extracted = piper_neo::extractPackage(info.modelPath, options.outputDir / "neo-cache");
    loadModelPath = extracted.modelPath;
    loadConfigPath = extracted.configPath;
  }

  piper::loadVoice(piperConfig, loadModelPath.string(), loadConfigPath.string(),
                   *voice, speakerId, options.useCuda, options.cpuThreads);

  const auto loadEnd = std::chrono::steady_clock::now();
  spdlog::info("{} Model load finished: model={} format={} duration_ms={}", nowIso8601(),
               info.name, info.format,
               std::chrono::duration_cast<std::chrono::milliseconds>(loadEnd - loadStart).count());

  return voice;
}

} // namespace piper_server
