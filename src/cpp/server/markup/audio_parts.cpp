#include "server/markup/audio_parts.hpp"

#include "server/wav_utils.hpp"

namespace piper_server {

void normalizeMarkupAudioSampleRates(std::vector<MarkupAudioPart> &parts, int targetSampleRate,
                                     int sampleWidth, int channels) {
  for (auto &part : parts) {
    if (!part.silence && part.sourceSampleRate != targetSampleRate) {
      part.pcm = resamplePcmS16Linear(part.pcm, part.sourceSampleRate, targetSampleRate, sampleWidth, channels);
      part.sourceSampleRate = targetSampleRate;
    }
  }
}

std::uint64_t markupAudioDataBytes(const std::vector<MarkupAudioPart> &parts, int targetSampleRate,
                                   int sampleWidth, int channels) {
  std::uint64_t totalDataBytes = 0;
  for (const auto &part : parts) {
    if (part.silence) {
      totalDataBytes += silenceBytesForMs(part.silenceMs, targetSampleRate, sampleWidth, channels);
    } else {
      totalDataBytes += part.pcm.size();
    }
  }
  return totalDataBytes;
}

json markupVoiceSettingsJson(const MarkupVoiceSettings &settings, const std::optional<std::string> &fallbackModel,
                             const std::optional<piper::SpeakerId> &fallbackSpeaker) {
  json out;
  out["model"] = settings.model.value_or(fallbackModel.value_or("<default>"));
  if (settings.speakerId) {
    out["speaker"] = *settings.speakerId;
  } else if (fallbackSpeaker) {
    out["speaker"] = *fallbackSpeaker;
  } else {
    out["speaker"] = nullptr;
  }
  if (settings.noiseScale) out["noise_scale"] = *settings.noiseScale;
  if (settings.lengthScale) out["length_scale"] = *settings.lengthScale;
  if (settings.noiseW) out["noise_w"] = *settings.noiseW;
  if (settings.sentenceSilenceSeconds) out["sentence_silence"] = *settings.sentenceSilenceSeconds;
  return out;
}

json writeMarkupAudioParts(std::ostream &output, const std::vector<MarkupAudioPart> &parts,
                           const std::optional<std::string> &defaultModel,
                           const std::optional<piper::SpeakerId> &defaultSpeakerId,
                           int targetSampleRate, int sampleWidth, int channels,
                           std::uint64_t &cursorMs) {
  json segmentsJson = json::array();
  for (const auto &part : parts) {
    const auto startMs = cursorMs;
    if (part.silence) {
      const auto zeroBytes = silenceBytesForMs(part.silenceMs, targetSampleRate, sampleWidth, channels);
      writeZeroBytes(output, zeroBytes);
      cursorMs += part.silenceMs;
      segmentsJson.push_back(json{{"type", "silence"},
                                  {"duration_ms", part.silenceMs},
                                  {"start_ms", startMs},
                                  {"end_ms", cursorMs}});
      continue;
    }

    output.write(part.pcm.data(), static_cast<std::streamsize>(part.pcm.size()));
    const auto blockBytesPerSecond = static_cast<double>(targetSampleRate * sampleWidth * channels);
    const auto durationMs = static_cast<std::uint64_t>((static_cast<double>(part.pcm.size()) / blockBytesPerSecond) * 1000.0 + 0.5);
    cursorMs += durationMs;
    auto settingsJson = markupVoiceSettingsJson(part.settings, defaultModel, defaultSpeakerId);
    settingsJson["resolved_model"] = part.modelName;
    segmentsJson.push_back(json{{"type", "speech"},
                                {"model", part.modelName},
                                {"voice", settingsJson},
                                {"text", part.text},
                                {"sample_rate", targetSampleRate},
                                {"start_ms", startMs},
                                {"end_ms", cursorMs}});
  }
  return segmentsJson;
}

} // namespace piper_server
