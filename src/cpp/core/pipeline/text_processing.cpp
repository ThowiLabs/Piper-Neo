#include "core/pipeline/text_processing.hpp"

#include "core/synthesis_utils.hpp"
#include "text_normalizer.hpp"

#include <mutex>
#include <stdexcept>

#include <spdlog/spdlog.h>

namespace piper::core::pipeline {
namespace {

std::mutex &globalTashkeelMutex() {
  static std::mutex mutex;
  return mutex;
}

} // namespace

std::string normalizeTextForPipeline(const Voice &voice, const std::string &text,
                                     bool allowTextNormalization) {
  if (!allowTextNormalization || !voice.textNormalizationConfig.enabled) {
    return text;
  }

  const auto normalizedText =
      normalizeTextForSpeech(text, voice.textNormalizationConfig);
  if (normalizedText != text) {
    spdlog::debug("Text normalized before phonemize: {}",
                  previewTextForLog(normalizedText));
  }

  return normalizedText;
}

std::string diacritizeTextForPipeline(PiperConfig &config, std::string text) {
  if (!config.useTashkeel) {
    return text;
  }

  if (!config.tashkeelState) {
    throw std::runtime_error("Tashkeel model is not loaded");
  }

  spdlog::debug("Diacritizing text with libtashkeel: {}",
                previewTextForLog(text));

  // libtashkeel keeps mutable model state behind the State object. The server can
  // process chunks in parallel, so guard this global-ish resource explicitly.
  std::lock_guard<std::mutex> lock(globalTashkeelMutex());
  return tashkeel::tashkeel_run(text, *config.tashkeelState);
}

} // namespace piper::core::pipeline
