#ifndef PIPER_SERVER_MARKUP_AUDIO_PARTS_H_
#define PIPER_SERVER_MARKUP_AUDIO_PARTS_H_

#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "../types.hpp"

namespace piper_server {

struct MarkupAudioPart {
  bool silence = false;
  std::uint64_t silenceMs = 0;
  std::string pcm;
  int sourceSampleRate = 0;
  std::string modelName;
  MarkupVoiceSettings settings;
  std::string text;
};

void normalizeMarkupAudioSampleRates(std::vector<MarkupAudioPart> &parts, int targetSampleRate,
                                     int sampleWidth, int channels);
std::uint64_t markupAudioDataBytes(const std::vector<MarkupAudioPart> &parts, int targetSampleRate,
                                   int sampleWidth, int channels);
json markupVoiceSettingsJson(const MarkupVoiceSettings &settings, const std::optional<std::string> &fallbackModel,
                             const std::optional<piper::SpeakerId> &fallbackSpeaker);
json writeMarkupAudioParts(std::ostream &output, const std::vector<MarkupAudioPart> &parts,
                           const std::optional<std::string> &defaultModel,
                           const std::optional<piper::SpeakerId> &defaultSpeakerId,
                           int targetSampleRate, int sampleWidth, int channels,
                           std::uint64_t &cursorMs);

} // namespace piper_server

#endif // PIPER_SERVER_MARKUP_AUDIO_PARTS_H_
