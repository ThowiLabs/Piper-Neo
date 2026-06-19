#ifndef PIPER_CORE_PIPELINE_PHRASE_SYNTHESIZER_HPP_
#define PIPER_CORE_PIPELINE_PHRASE_SYNTHESIZER_HPP_

#include "piper/types.hpp"

#include <cstddef>
#include <functional>
#include <map>
#include <vector>

namespace piper::core::pipeline {

struct PhrasePhonemes {
  std::vector<Phoneme> phonemes;
  std::size_t silenceSamplesAfter = 0;
};

std::vector<PhrasePhonemes> splitSentenceIntoPhrases(
    const std::vector<Phoneme> &sentencePhonemes,
    const SynthesisConfig &synthesisConfig);

void synthesizeSentencePhonemes(
    Voice &voice, const std::vector<Phoneme> &sentencePhonemes,
    PhonemeIdConfig &idConfig,
    std::map<Phoneme, std::size_t> &missingPhonemes,
    std::vector<int16_t> &audioBuffer, SynthesisResult &result,
    const std::function<bool()> &shouldCancel);

void logMissingPhonemes(const std::map<Phoneme, std::size_t> &missingPhonemes);

void updateRealTimeFactor(SynthesisResult &result);

} // namespace piper::core::pipeline

#endif // PIPER_CORE_PIPELINE_PHRASE_SYNTHESIZER_HPP_
