#include "core/pipeline/phonemizer.hpp"

#include "core/synthesis_utils.hpp"

#include <mutex>

#include <spdlog/spdlog.h>

namespace piper::core::pipeline {
namespace {

std::mutex &globalPhonemizeMutex() {
  static std::mutex mutex;
  return mutex;
}

} // namespace

std::vector<std::vector<Phoneme>> phonemizeTextForPipeline(
    const Voice &voice, const std::string &text) {
  spdlog::debug("Phonemizing text: {}", previewTextForLog(text));

  std::vector<std::vector<Phoneme>> phonemes;
  if (voice.phonemizeConfig.phonemeType == eSpeakPhonemes) {
    eSpeakPhonemeConfig eSpeakConfig;
    eSpeakConfig.voice = voice.phonemizeConfig.eSpeak.voice;

    // eSpeak-ng/piper-phonemize uses process-global dictionary state. Parallel
    // phonemization can corrupt reads on Windows (for example: es_dict length=0).
    // Keep the lock scoped to phonemization so ONNX inference can still run in
    // parallel across model replicas.
    std::lock_guard<std::mutex> lock(globalPhonemizeMutex());
    phonemize_eSpeak(text, eSpeakConfig, phonemes);
  } else {
    CodepointsPhonemeConfig codepointsConfig;
    phonemize_codepoints(text, codepointsConfig, phonemes);
  }

  return phonemes;
}

} // namespace piper::core::pipeline
