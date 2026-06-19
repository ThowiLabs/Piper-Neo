#ifndef PIPER_SERVER_MODEL_RUNTIME_H_
#define PIPER_SERVER_MODEL_RUNTIME_H_

#include <condition_variable>
#include <memory>
#include <mutex>
#include <vector>

#include "../server.hpp"
#include "piper/types.hpp"

namespace piper_server {

struct VoiceSlot {
  std::unique_ptr<piper::Voice> ownedVoice;
  piper::Voice *voice = nullptr;
  bool inUse = false;
};

struct ModelRuntime {
  explicit ModelRuntime(ModelInfo modelInfo);

  ModelInfo info;
  std::mutex mutex;
  std::condition_variable cv;
  std::vector<std::unique_ptr<VoiceSlot>> slots;
  std::size_t loadingSlots = 0;
};

struct VoiceLease {
  std::shared_ptr<ModelRuntime> runtime;
  VoiceSlot *slot = nullptr;

  VoiceLease() = default;
  VoiceLease(std::shared_ptr<ModelRuntime> modelRuntime, VoiceSlot *voiceSlot);
  VoiceLease(const VoiceLease &) = delete;
  VoiceLease &operator=(const VoiceLease &) = delete;
  VoiceLease(VoiceLease &&other) noexcept;
  VoiceLease &operator=(VoiceLease &&other) noexcept;
  ~VoiceLease();

  piper::Voice &get() const;
  const ModelInfo &model() const;
  explicit operator bool() const;
  void reset();
};

} // namespace piper_server

#endif // PIPER_SERVER_MODEL_RUNTIME_H_
