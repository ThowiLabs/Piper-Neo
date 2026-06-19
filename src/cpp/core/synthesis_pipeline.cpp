#include "piper/api.hpp"

#include "core/pipeline/phonemizer.hpp"
#include "core/pipeline/phrase_synthesizer.hpp"
#include "core/pipeline/text_processing.hpp"
#include "core/sentence_splitter.hpp"
#include "core/synthesis_utils.hpp"

#include <map>
#include <memory>
#include <utility>
#include <vector>

namespace piper {
namespace {

class SentenceSilenceGuard {
public:
  explicit SentenceSilenceGuard(SynthesisConfig &config)
      : config_(config), previous_(config.sentenceSilenceSeconds) {}

  void set(float value) { config_.sentenceSilenceSeconds = value; }

  ~SentenceSilenceGuard() { config_.sentenceSilenceSeconds = previous_; }

private:
  SynthesisConfig &config_;
  float previous_;
};

void flushAudioIfNeeded(std::vector<int16_t> &audioBuffer,
                        const std::function<void()> &audioCallback) {
  if (!audioCallback) {
    return;
  }

  audioCallback();
  audioBuffer.clear();
}

PhonemeIdConfig makePhonemeIdConfig(const Voice &voice) {
  PhonemeIdConfig idConfig;
  idConfig.phonemeIdMap =
      std::make_shared<PhonemeIdMap>(voice.phonemizeConfig.phonemeIdMap);
  return idConfig;
}

void textToAudioInternal(PiperConfig &config, Voice &voice, std::string text,
                         std::vector<int16_t> &audioBuffer,
                         SynthesisResult &result,
                         const std::function<void()> &audioCallback,
                         const std::function<bool()> &shouldCancel,
                         bool allowTextNormalization) {
  core::throwIfSynthesisCancelled(shouldCancel);

  text = core::pipeline::normalizeTextForPipeline(voice, text,
                                                  allowTextNormalization);

  const float explicitSentenceSilenceSeconds =
      voice.synthesisConfig.sentenceSilenceSeconds;
  if (explicitSentenceSilenceSeconds > 0) {
    auto explicitChunks = core::splitTextIntoExplicitSentenceChunks(text);
    if (explicitChunks.size() > 1) {
      SentenceSilenceGuard silenceGuard(voice.synthesisConfig);
      silenceGuard.set(0.0f);

      const auto explicitSilenceSamples = core::secondsToSamples(
          explicitSentenceSilenceSeconds, voice.synthesisConfig.sampleRate,
          voice.synthesisConfig.channels);

      for (const auto &chunk : explicitChunks) {
        core::throwIfSynthesisCancelled(shouldCancel);

        SynthesisResult chunkResult;
        textToAudioInternal(config, voice, chunk.text, audioBuffer, chunkResult,
                            audioCallback, shouldCancel, false);
        result.audioSeconds += chunkResult.audioSeconds;
        result.inferSeconds += chunkResult.inferSeconds;

        if (chunk.addSilenceAfter && (explicitSilenceSamples > 0)) {
          core::appendSilenceSamples(audioBuffer, explicitSilenceSamples);
          result.audioSeconds += explicitSentenceSilenceSeconds;
          flushAudioIfNeeded(audioBuffer, audioCallback);
        }
      }

      core::pipeline::updateRealTimeFactor(result);
      return;
    }
  }

  const auto sentenceSilenceSamples = core::secondsToSamples(
      voice.synthesisConfig.sentenceSilenceSeconds,
      voice.synthesisConfig.sampleRate, voice.synthesisConfig.channels);

  text = core::pipeline::diacritizeTextForPipeline(config, std::move(text));
  core::throwIfSynthesisCancelled(shouldCancel);

  auto phonemeSentences = core::pipeline::phonemizeTextForPipeline(voice, text);
  auto idConfig = makePhonemeIdConfig(voice);
  std::map<Phoneme, std::size_t> missingPhonemes;

  for (const auto &sentencePhonemes : phonemeSentences) {
    core::pipeline::synthesizeSentencePhonemes(
        voice, sentencePhonemes, idConfig, missingPhonemes, audioBuffer, result,
        shouldCancel);

    if (sentenceSilenceSamples > 0) {
      core::appendSilenceSamples(audioBuffer, sentenceSilenceSamples);
      result.audioSeconds += voice.synthesisConfig.sentenceSilenceSeconds;
    }

    core::throwIfSynthesisCancelled(shouldCancel);
    flushAudioIfNeeded(audioBuffer, audioCallback);
  }

  core::pipeline::logMissingPhonemes(missingPhonemes);
  core::pipeline::updateRealTimeFactor(result);
}

} // namespace

void textToAudio(PiperConfig &config, Voice &voice, std::string text,
                 std::vector<int16_t> &audioBuffer, SynthesisResult &result,
                 const std::function<void()> &audioCallback,
                 const std::function<bool()> &shouldCancel) {
  textToAudioInternal(config, voice, std::move(text), audioBuffer, result,
                      audioCallback, shouldCancel, true);
}

} // namespace piper
