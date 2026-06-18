#include "piper/api.hpp"

#include "core/model_runtime.hpp"
#include "core/sentence_splitter.hpp"
#include "core/synthesis_utils.hpp"

#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

#include "text_normalizer.hpp"
#include "utf8.h"

namespace piper {
namespace {

struct PhrasePhonemes {
  std::vector<Phoneme> phonemes;
  std::size_t silenceSamplesAfter = 0;
};

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

std::string maybeNormalizeText(const Voice &voice, const std::string &text,
                               bool allowTextNormalization) {
  if (!allowTextNormalization || !voice.textNormalizationConfig.enabled) {
    return text;
  }

  const auto normalizedText =
      normalizeTextForSpeech(text, voice.textNormalizationConfig);
  if (normalizedText != text) {
    spdlog::debug("Text normalized before phonemize: {}",
                  core::previewTextForLog(normalizedText));
  }

  return normalizedText;
}

std::string maybeDiacritizeText(PiperConfig &config, std::string text) {
  if (!config.useTashkeel) {
    return text;
  }

  if (!config.tashkeelState) {
    throw std::runtime_error("Tashkeel model is not loaded");
  }

  spdlog::debug("Diacritizing text with libtashkeel: {}",
                core::previewTextForLog(text));
  return tashkeel::tashkeel_run(text, *config.tashkeelState);
}

std::vector<std::vector<Phoneme>> phonemizeText(const Voice &voice,
                                                const std::string &text) {
  spdlog::debug("Phonemizing text: {}", core::previewTextForLog(text));

  std::vector<std::vector<Phoneme>> phonemes;
  if (voice.phonemizeConfig.phonemeType == eSpeakPhonemes) {
    eSpeakPhonemeConfig eSpeakConfig;
    eSpeakConfig.voice = voice.phonemizeConfig.eSpeak.voice;
    phonemize_eSpeak(text, eSpeakConfig, phonemes);
  } else {
    CodepointsPhonemeConfig codepointsConfig;
    phonemize_codepoints(text, codepointsConfig, phonemes);
  }

  return phonemes;
}

std::vector<PhrasePhonemes> splitSentenceIntoPhrases(
    const std::vector<Phoneme> &sentencePhonemes,
    const SynthesisConfig &synthesisConfig) {
  std::vector<PhrasePhonemes> phrases;

  if (!synthesisConfig.phonemeSilenceSeconds) {
    phrases.push_back({sentencePhonemes, 0});
    return phrases;
  }

  const auto &phonemeSilenceSeconds = *synthesisConfig.phonemeSilenceSeconds;
  PhrasePhonemes currentPhrase;

  for (auto currentPhoneme : sentencePhonemes) {
    currentPhrase.phonemes.push_back(currentPhoneme);

    auto silenceIter = phonemeSilenceSeconds.find(currentPhoneme);
    if (silenceIter == phonemeSilenceSeconds.end()) {
      continue;
    }

    currentPhrase.silenceSamplesAfter = core::secondsToSamples(
        silenceIter->second, synthesisConfig.sampleRate, synthesisConfig.channels);
    phrases.push_back(std::move(currentPhrase));
    currentPhrase = PhrasePhonemes{};
  }

  if (!currentPhrase.phonemes.empty()) {
    phrases.push_back(std::move(currentPhrase));
  }

  return phrases;
}

void logPhonemesForDebug(const std::vector<Phoneme> &sentencePhonemes) {
  if (!spdlog::should_log(spdlog::level::debug)) {
    return;
  }

  std::string phonemesStr;
  for (auto phoneme : sentencePhonemes) {
    utf8::append(phoneme, std::back_inserter(phonemesStr));
  }

  spdlog::debug("Converting {} phoneme(s) to ids: {}",
                sentencePhonemes.size(), phonemesStr);
}

void logPhonemeIdsForDebug(std::size_t phonemeCount,
                            const std::vector<PhonemeId> &phonemeIds) {
  if (!spdlog::should_log(spdlog::level::debug)) {
    return;
  }

  std::stringstream phonemeIdsStr;
  for (auto phonemeId : phonemeIds) {
    phonemeIdsStr << phonemeId << ", ";
  }

  spdlog::debug("Converted {} phoneme(s) to {} phoneme id(s): {}", phonemeCount,
                phonemeIds.size(), phonemeIdsStr.str());
}

void logMissingPhonemes(const std::map<Phoneme, std::size_t> &missingPhonemes) {
  if (missingPhonemes.empty()) {
    return;
  }

  spdlog::warn("Missing {} phoneme(s) from phoneme/id map!",
               missingPhonemes.size());

  for (auto phonemeCount : missingPhonemes) {
    std::string phonemeStr;
    utf8::append(phonemeCount.first, std::back_inserter(phonemeStr));
    spdlog::warn("Missing \"{}\" (\\u{:04X}): {} time(s)", phonemeStr,
                 static_cast<uint32_t>(phonemeCount.first), phonemeCount.second);
  }
}

void updateRealTimeFactor(SynthesisResult &result) {
  if (result.audioSeconds > 0) {
    result.realTimeFactor = result.inferSeconds / result.audioSeconds;
  }
}

void synthesizePhrases(Voice &voice, const std::vector<PhrasePhonemes> &phrases,
                       PhonemeIdConfig &idConfig,
                       std::map<Phoneme, std::size_t> &missingPhonemes,
                       std::vector<int16_t> &audioBuffer,
                       SynthesisResult &result,
                       const std::function<bool()> &shouldCancel) {
  std::vector<PhonemeId> phonemeIds;

  for (const auto &phrase : phrases) {
    core::throwIfSynthesisCancelled(shouldCancel);
    if (phrase.phonemes.empty()) {
      continue;
    }

    phonemes_to_ids(phrase.phonemes, idConfig, phonemeIds, missingPhonemes);
    logPhonemeIdsForDebug(phrase.phonemes.size(), phonemeIds);

    SynthesisResult phraseResult;
    core::synthesizePhonemeIds(phonemeIds, voice.synthesisConfig,
                               voice.session, audioBuffer, phraseResult);
    core::throwIfSynthesisCancelled(shouldCancel);

    core::appendSilenceSamples(audioBuffer, phrase.silenceSamplesAfter);

    result.audioSeconds += phraseResult.audioSeconds;
    result.inferSeconds += phraseResult.inferSeconds;

    phonemeIds.clear();
  }
}

void flushAudioIfNeeded(std::vector<int16_t> &audioBuffer,
                        const std::function<void()> &audioCallback) {
  if (!audioCallback) {
    return;
  }

  audioCallback();
  audioBuffer.clear();
}

void textToAudioInternal(PiperConfig &config, Voice &voice, std::string text,
                         std::vector<int16_t> &audioBuffer,
                         SynthesisResult &result,
                         const std::function<void()> &audioCallback,
                         const std::function<bool()> &shouldCancel,
                         bool allowTextNormalization) {
  core::throwIfSynthesisCancelled(shouldCancel);

  text = maybeNormalizeText(voice, text, allowTextNormalization);

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

      updateRealTimeFactor(result);
      return;
    }
  }

  const auto sentenceSilenceSamples = core::secondsToSamples(
      voice.synthesisConfig.sentenceSilenceSeconds,
      voice.synthesisConfig.sampleRate, voice.synthesisConfig.channels);

  text = maybeDiacritizeText(config, std::move(text));
  core::throwIfSynthesisCancelled(shouldCancel);

  auto phonemes = phonemizeText(voice, text);

  auto idMap = std::make_shared<PhonemeIdMap>(voice.phonemizeConfig.phonemeIdMap);
  PhonemeIdConfig idConfig;
  idConfig.phonemeIdMap = idMap;

  std::map<Phoneme, std::size_t> missingPhonemes;
  for (const auto &sentencePhonemes : phonemes) {
    core::throwIfSynthesisCancelled(shouldCancel);
    logPhonemesForDebug(sentencePhonemes);

    const auto phrases = splitSentenceIntoPhrases(sentencePhonemes,
                                                  voice.synthesisConfig);
    synthesizePhrases(voice, phrases, idConfig, missingPhonemes, audioBuffer,
                      result, shouldCancel);

    if (sentenceSilenceSamples > 0) {
      core::appendSilenceSamples(audioBuffer, sentenceSilenceSamples);
      result.audioSeconds += voice.synthesisConfig.sentenceSilenceSeconds;
    }

    core::throwIfSynthesisCancelled(shouldCancel);
    flushAudioIfNeeded(audioBuffer, audioCallback);
  }

  logMissingPhonemes(missingPhonemes);
  updateRealTimeFactor(result);
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
