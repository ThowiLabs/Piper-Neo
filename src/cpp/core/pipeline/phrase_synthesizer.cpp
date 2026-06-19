#include "core/pipeline/phrase_synthesizer.hpp"

#include "core/model_runtime.hpp"
#include "core/synthesis_utils.hpp"

#include <sstream>
#include <utility>

#include <spdlog/spdlog.h>

#include "utf8.h"

namespace piper::core::pipeline {
namespace {

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

void synthesizePhrases(Voice &voice, const std::vector<PhrasePhonemes> &phrases,
                       PhonemeIdConfig &idConfig,
                       std::map<Phoneme, std::size_t> &missingPhonemes,
                       std::vector<int16_t> &audioBuffer,
                       SynthesisResult &result,
                       const std::function<bool()> &shouldCancel) {
  std::vector<PhonemeId> phonemeIds;

  for (const auto &phrase : phrases) {
    throwIfSynthesisCancelled(shouldCancel);
    if (phrase.phonemes.empty()) {
      continue;
    }

    phonemes_to_ids(phrase.phonemes, idConfig, phonemeIds, missingPhonemes);
    logPhonemeIdsForDebug(phrase.phonemes.size(), phonemeIds);

    SynthesisResult phraseResult;
    synthesizePhonemeIds(phonemeIds, voice.synthesisConfig, voice.session,
                         audioBuffer, phraseResult);
    throwIfSynthesisCancelled(shouldCancel);

    appendSilenceSamples(audioBuffer, phrase.silenceSamplesAfter);

    result.audioSeconds += phraseResult.audioSeconds;
    result.inferSeconds += phraseResult.inferSeconds;

    phonemeIds.clear();
  }
}

} // namespace

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

    currentPhrase.silenceSamplesAfter = secondsToSamples(
        silenceIter->second, synthesisConfig.sampleRate, synthesisConfig.channels);
    phrases.push_back(std::move(currentPhrase));
    currentPhrase = PhrasePhonemes{};
  }

  if (!currentPhrase.phonemes.empty()) {
    phrases.push_back(std::move(currentPhrase));
  }

  return phrases;
}

void synthesizeSentencePhonemes(
    Voice &voice, const std::vector<Phoneme> &sentencePhonemes,
    PhonemeIdConfig &idConfig,
    std::map<Phoneme, std::size_t> &missingPhonemes,
    std::vector<int16_t> &audioBuffer, SynthesisResult &result,
    const std::function<bool()> &shouldCancel) {
  throwIfSynthesisCancelled(shouldCancel);
  logPhonemesForDebug(sentencePhonemes);

  const auto phrases = splitSentenceIntoPhrases(sentencePhonemes,
                                                voice.synthesisConfig);
  synthesizePhrases(voice, phrases, idConfig, missingPhonemes, audioBuffer,
                    result, shouldCancel);
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

} // namespace piper::core::pipeline
