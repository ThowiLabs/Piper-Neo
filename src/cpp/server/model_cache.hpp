#ifndef PIPER_SERVER_MODEL_CACHE_H_
#define PIPER_SERVER_MODEL_CACHE_H_

#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include "../server.hpp"
#include "model_registry.hpp"
#include "model_runtime.hpp"

namespace piper_server {

class ModelCache {
public:
  ModelCache(piper::PiperConfig &piperConfig, piper::Voice &defaultVoice,
             const ServerOptions &options, ModelRegistry &registry);

  VoiceLease checkout(const std::optional<std::string> &requestedModel);

private:
  ModelInfo resolve(const std::optional<std::string> &requestedModel);
  std::shared_ptr<ModelRuntime> runtimeFor(const ModelInfo &info);

  piper::PiperConfig &piperConfig;
  const ServerOptions &options;
  ModelRegistry &registry;
  const std::size_t maxReplicas;
  std::mutex cacheMutex;
  std::map<std::string, std::shared_ptr<ModelRuntime>> cache;
};

} // namespace piper_server

#endif // PIPER_SERVER_MODEL_CACHE_H_
