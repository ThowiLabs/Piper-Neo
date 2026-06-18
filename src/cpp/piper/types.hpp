#ifndef PIPER_TYPES_H_
#define PIPER_TYPES_H_

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <onnxruntime_cxx_api.h>
#include <piper-phonemize/phoneme_ids.hpp>
#include <piper-phonemize/phonemize.hpp>
#include <piper-phonemize/tashkeel.hpp>

#include "../json.hpp"
#include "../text_normalizer.hpp"

using json = nlohmann::json;

namespace piper {

typedef int64_t SpeakerId;

struct eSpeakConfig {
  std::string voice = "en-us";
};

struct PiperConfig {
  std::string eSpeakDataPath;
  bool useESpeak = true;

  bool useTashkeel = false;
  std::optional<std::string> tashkeelModelPath;
  std::unique_ptr<tashkeel::State> tashkeelState;
};

enum PhonemeType { eSpeakPhonemes, TextPhonemes };

struct PhonemizeConfig {
  PhonemeType phonemeType = eSpeakPhonemes;
  std::optional<std::map<Phoneme, std::vector<Phoneme>>> phonemeMap;
  std::map<Phoneme, std::vector<PhonemeId>> phonemeIdMap;

  PhonemeId idPad = 0;
  PhonemeId idBos = 1;
  PhonemeId idEos = 2;
  bool interspersePad = true;

  eSpeakConfig eSpeak;
};

struct SynthesisConfig {
  float noiseScale = 0.667f;
  float lengthScale = 1.0f;
  float noiseW = 0.8f;

  int sampleRate = 22050;
  int sampleWidth = 2;
  int channels = 1;

  std::optional<SpeakerId> speakerId;

  float sentenceSilenceSeconds = 0.35f;
  std::optional<std::map<piper::Phoneme, float>> phonemeSilenceSeconds;
};

struct ModelConfig {
  int numSpeakers;
  std::optional<std::map<std::string, SpeakerId>> speakerIdMap;
};

struct ModelSession {
  Ort::Session onnx;
  Ort::AllocatorWithDefaultOptions allocator;
  Ort::SessionOptions options;
  Ort::Env env;

  ModelSession() : onnx(nullptr){};
};

struct SynthesisResult {
  double inferSeconds = 0.0;
  double audioSeconds = 0.0;
  double realTimeFactor = 0.0;
};

struct Voice {
  json configRoot;
  PhonemizeConfig phonemizeConfig;
  SynthesisConfig synthesisConfig;
  ModelConfig modelConfig;
  TextNormalizationConfig textNormalizationConfig;
  ModelSession session;
};

} // namespace piper

#endif // PIPER_TYPES_H_
