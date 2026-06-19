#include "server/model_runtime.hpp"

#include <utility>

namespace piper_server {

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

} // namespace piper_server
