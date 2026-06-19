#ifndef PIPER_SERVER_MODEL_REGISTRY_H_
#define PIPER_SERVER_MODEL_REGISTRY_H_

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "../server.hpp"
#include "types.hpp"
#include "model_metadata.hpp"
#include "model_scanner.hpp"

namespace piper_server {

std::optional<ModelInfo> findModelByName(const ServerOptions &options,
                                         const std::string &requestedModel,
                                         class ModelRegistry *registry = nullptr);

class ModelRegistry {
public:
  explicit ModelRegistry(const ServerOptions &options);

  json list(const std::string &includeMode);
  std::optional<ModelInfo> find(const std::string &modelName);
  void forceRefresh();

private:
  void refreshIfNeeded();
  void refreshLocked();

  const ServerOptions &options;
  const std::size_t refreshSeconds;
  std::mutex mutex;
  bool initialized = false;
  std::chrono::steady_clock::time_point nextRefresh{};
  std::vector<ModelInfo> cachedModels;
};

} // namespace piper_server

#endif // PIPER_SERVER_MODEL_REGISTRY_H_
