#include "piper/api.hpp"

#include "core/model_runtime.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

#include <spdlog/spdlog.h>

#include "text_normalizer.hpp"
#include "utf8.h"

namespace piper {
namespace {

void parsePhonemizeConfig(json &configRoot, PhonemizeConfig &phonemizeConfig) {
  if (configRoot.contains("espeak")) {
    auto espeakValue = configRoot["espeak"];
    if (espeakValue.contains("voice")) {
      phonemizeConfig.eSpeak.voice = espeakValue["voice"].get<std::string>();
    }
  }

  if (configRoot.contains("phoneme_type")) {
    auto phonemeTypeStr = configRoot["phoneme_type"].get<std::string>();
    if (phonemeTypeStr == "text") {
      phonemizeConfig.phonemeType = TextPhonemes;
    }
  }

  if (configRoot.contains("phoneme_id_map")) {
    auto phonemeIdMapValue = configRoot["phoneme_id_map"];
    for (auto &fromPhonemeItem : phonemeIdMapValue.items()) {
      std::string fromPhoneme = fromPhonemeItem.key();
      if (!isSingleCodepoint(fromPhoneme)) {
        std::stringstream idsStr;
        for (auto &toIdValue : fromPhonemeItem.value()) {
          PhonemeId toId = toIdValue.get<PhonemeId>();
          idsStr << toId << ",";
        }

        spdlog::error("\"{}\" is not a single codepoint (ids={})", fromPhoneme,
                      idsStr.str());
        throw std::runtime_error(
            "Phonemes must be one codepoint (phoneme id map)");
      }

      auto fromCodepoint = getCodepoint(fromPhoneme);
      for (auto &toIdValue : fromPhonemeItem.value()) {
        PhonemeId toId = toIdValue.get<PhonemeId>();
        phonemizeConfig.phonemeIdMap[fromCodepoint].push_back(toId);
      }
    }
  }

  if (configRoot.contains("phoneme_map")) {
    if (!phonemizeConfig.phonemeMap) {
      phonemizeConfig.phonemeMap.emplace();
    }

    auto phonemeMapValue = configRoot["phoneme_map"];
    for (auto &fromPhonemeItem : phonemeMapValue.items()) {
      std::string fromPhoneme = fromPhonemeItem.key();
      if (!isSingleCodepoint(fromPhoneme)) {
        spdlog::error("\"{}\" is not a single codepoint", fromPhoneme);
        throw std::runtime_error("Phonemes must be one codepoint (phoneme map)");
      }

      auto fromCodepoint = getCodepoint(fromPhoneme);
      for (auto &toPhonemeValue : fromPhonemeItem.value()) {
        std::string toPhoneme = toPhonemeValue.get<std::string>();
        if (!isSingleCodepoint(toPhoneme)) {
          throw std::runtime_error("Phonemes must be one codepoint (phoneme map)");
        }

        auto toCodepoint = getCodepoint(toPhoneme);
        (*phonemizeConfig.phonemeMap)[fromCodepoint].push_back(toCodepoint);
      }
    }
  }
}

void parseSynthesisConfig(json &configRoot, SynthesisConfig &synthesisConfig) {
  if (configRoot.contains("audio")) {
    auto audioValue = configRoot["audio"];
    if (audioValue.contains("sample_rate")) {
      synthesisConfig.sampleRate = audioValue.value("sample_rate", 22050);
    }
  }

  if (configRoot.contains("inference")) {
    auto inferenceValue = configRoot["inference"];
    if (inferenceValue.contains("noise_scale")) {
      synthesisConfig.noiseScale = inferenceValue.value("noise_scale", 0.667f);
    }

    if (inferenceValue.contains("length_scale")) {
      synthesisConfig.lengthScale = inferenceValue.value("length_scale", 1.0f);
    }

    if (inferenceValue.contains("noise_w")) {
      synthesisConfig.noiseW = inferenceValue.value("noise_w", 0.8f);
    }

    if (inferenceValue.contains("phoneme_silence")) {
      synthesisConfig.phonemeSilenceSeconds.emplace();
      auto phonemeSilenceValue = inferenceValue["phoneme_silence"];
      for (auto &phonemeItem : phonemeSilenceValue.items()) {
        std::string phonemeStr = phonemeItem.key();
        if (!isSingleCodepoint(phonemeStr)) {
          spdlog::error("\"{}\" is not a single codepoint", phonemeStr);
          throw std::runtime_error(
              "Phonemes must be one codepoint (phoneme silence)");
        }

        auto phoneme = getCodepoint(phonemeStr);
        (*synthesisConfig.phonemeSilenceSeconds)[phoneme] =
            phonemeItem.value().get<float>();
      }
    }
  }
}

void parseModelConfig(json &configRoot, ModelConfig &modelConfig) {
  modelConfig.numSpeakers = configRoot["num_speakers"].get<SpeakerId>();

  if (configRoot.contains("speaker_id_map")) {
    if (!modelConfig.speakerIdMap) {
      modelConfig.speakerIdMap.emplace();
    }

    auto speakerIdMapValue = configRoot["speaker_id_map"];
    for (auto &speakerItem : speakerIdMapValue.items()) {
      std::string speakerName = speakerItem.key();
      (*modelConfig.speakerIdMap)[speakerName] =
          speakerItem.value().get<SpeakerId>();
    }
  }
}

} // namespace

bool isSingleCodepoint(std::string s) {
  return utf8::distance(s.begin(), s.end()) == 1;
}

Phoneme getCodepoint(std::string s) {
  utf8::iterator characterIter(s.begin(), s.begin(), s.end());
  return *characterIter;
}

void loadVoice(PiperConfig &config, std::string modelPath,
               std::string modelConfigPath, Voice &voice,
               const std::optional<SpeakerId> &speakerId, bool useCuda,
               std::optional<int> cpuThreads) {
  spdlog::debug("Parsing voice config at {}", modelConfigPath);
  std::ifstream modelConfigFile(modelConfigPath);
  if (!modelConfigFile.good()) {
    throw std::runtime_error("Unable to open voice config: " + modelConfigPath);
  }

  voice.configRoot = json::parse(modelConfigFile);

  parsePhonemizeConfig(voice.configRoot, voice.phonemizeConfig);
  parseSynthesisConfig(voice.configRoot, voice.synthesisConfig);
  parseModelConfig(voice.configRoot, voice.modelConfig);
  parseTextNormalizationConfig(voice.configRoot, voice.textNormalizationConfig);

  if (voice.modelConfig.numSpeakers > 1) {
    if (speakerId) {
      voice.synthesisConfig.speakerId = speakerId;
    } else {
      voice.synthesisConfig.speakerId = 0;
    }
  }

  spdlog::debug("Voice contains {} speaker(s)", voice.modelConfig.numSpeakers);

  core::loadModel(modelPath, voice.session, useCuda, cpuThreads);
}

} // namespace piper
