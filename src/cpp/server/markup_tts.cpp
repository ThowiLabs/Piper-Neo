#include "markup_tts.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <optional>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "text_sanitizer.hpp"
#include "utils.hpp"
#include "wav_utils.hpp"

namespace piper_server {

bool looksLikeMarkupTts(const std::string &text) {
  const auto lower = lowerCopy(text);
  return lower.find("<model=") != std::string::npos ||
         lower.find("<model ") != std::string::npos ||
         lower.find("</model>") != std::string::npos ||
         lower.find("<silence") != std::string::npos;
}

std::map<std::string, std::string> parseLooseTagAttributes(const std::string &tagBody) {
  std::map<std::string, std::string> attrs;
  static const std::regex attrRegex(R"(([A-Za-z_][A-Za-z0-9_-]*)\s*=\s*(?:\"([^\"]*)\"|'([^']*)'))");
  for (std::sregex_iterator it(tagBody.begin(), tagBody.end(), attrRegex), end; it != end; ++it) {
    const auto key = lowerCopy((*it)[1].str());
    const auto value = (*it)[2].matched ? (*it)[2].str() : (*it)[3].str();
    attrs[key] = value;
  }
  return attrs;
}

std::optional<float> parseMarkupFloat(const std::string &value, const std::string &field,
                                      float minValue, float maxValue) {
  try {
    std::size_t used = 0;
    const auto parsed = std::stof(trimCopy(value), &used);
    if (used != trimCopy(value).size() || parsed < minValue || parsed > maxValue) {
      throw std::runtime_error("invalid");
    }
    return parsed;
  } catch (...) {
    throw std::runtime_error("markup_invalid_" + field);
  }
}

std::optional<piper::SpeakerId> parseMarkupSpeaker(const std::string &value) {
  try {
    std::size_t used = 0;
    const auto trimmed = trimCopy(value);
    const auto parsed = std::stoll(trimmed, &used);
    if (used != trimmed.size() || parsed < 0) {
      throw std::runtime_error("invalid");
    }
    return static_cast<piper::SpeakerId>(parsed);
  } catch (...) {
    throw std::runtime_error("markup_invalid_speaker");
  }
}

std::optional<std::uint64_t> parseSilenceDurationMs(const std::map<std::string, std::string> &attrs) {
  auto byKey = [&](const std::string &key) -> std::optional<std::string> {
    const auto it = attrs.find(key);
    if (it == attrs.end()) return std::nullopt;
    return trimCopy(it->second);
  };

  if (auto value = byKey("ms")) {
    auto parsed = parseMarkupFloat(*value, "silence", 0.0f, 60.0f * 60.0f * 1000.0f);
    return static_cast<std::uint64_t>(*parsed + 0.5f);
  }

  if (auto value = byKey("sec")) {
    auto parsed = parseMarkupFloat(*value, "silence", 0.0f, 60.0f * 60.0f);
    return static_cast<std::uint64_t>((*parsed * 1000.0f) + 0.5f);
  }

  if (auto value = byKey("seconds")) {
    auto parsed = parseMarkupFloat(*value, "silence", 0.0f, 60.0f * 60.0f);
    return static_cast<std::uint64_t>((*parsed * 1000.0f) + 0.5f);
  }

  if (auto value = byKey("silence")) {
    auto v = lowerCopy(trimCopy(*value));
    if (v.size() > 2 && v.substr(v.size() - 2) == "ms") {
      auto parsed = parseMarkupFloat(v.substr(0, v.size() - 2), "silence", 0.0f, 60.0f * 60.0f * 1000.0f);
      return static_cast<std::uint64_t>(*parsed + 0.5f);
    }
    if (v.size() > 1 && v.back() == 's') {
      auto parsed = parseMarkupFloat(v.substr(0, v.size() - 1), "silence", 0.0f, 60.0f * 60.0f);
      return static_cast<std::uint64_t>((*parsed * 1000.0f) + 0.5f);
    }
    auto parsed = parseMarkupFloat(v, "silence", 0.0f, 60.0f * 60.0f * 1000.0f);
    return static_cast<std::uint64_t>(*parsed + 0.5f);
  }

  return std::nullopt;
}

MarkupVoiceSettings parseMarkupModelSettings(const std::map<std::string, std::string> &attrs) {
  const auto modelIt = attrs.find("model");
  if (modelIt == attrs.end() || trimCopy(modelIt->second).empty()) {
    throw std::runtime_error("markup_model_missing");
  }

  MarkupVoiceSettings settings;
  auto modelName = trimCopy(modelIt->second);
  const auto hashPos = modelName.rfind('#');
  if (hashPos != std::string::npos && hashPos + 1 < modelName.size()) {
    settings.speakerId = parseMarkupSpeaker(modelName.substr(hashPos + 1));
    modelName = trimCopy(modelName.substr(0, hashPos));
  }
  settings.model = modelName;

  auto speakerIt = attrs.find("speaker");
  if (speakerIt == attrs.end()) {
    speakerIt = attrs.find("speacker");
  }
  if (speakerIt != attrs.end()) {
    settings.speakerId = parseMarkupSpeaker(speakerIt->second);
  }

  auto setFloat = [&](const std::vector<std::string> &keys, std::optional<float> &target,
                      const std::string &field, float minValue, float maxValue) {
    for (const auto &key : keys) {
      const auto it = attrs.find(key);
      if (it != attrs.end()) {
        target = parseMarkupFloat(it->second, field, minValue, maxValue);
        return;
      }
    }
  };

  setFloat({"noise_scale", "noisescale"}, settings.noiseScale, "noise_scale", 0.0f, 2.0f);
  setFloat({"length_scale", "lengthscale"}, settings.lengthScale, "length_scale", 0.05f, 10.0f);
  setFloat({"noise_w", "noisew"}, settings.noiseW, "noise_w", 0.0f, 2.0f);
  setFloat({"sentence_silence", "sentence_silence_sec", "sentence_silence_seconds", "sentencesilence"},
           settings.sentenceSilenceSeconds, "sentence_silence", 0.0f, 30.0f);

  return settings;
}

MarkupParseResult parseMarkupScript(const std::string &text) {
  MarkupParseResult result;
  MarkupVoiceSettings activeVoice;
  std::string buffer;

  auto flushSpeech = [&]() {
    const auto spoken = trimCopy(buffer);
    if (!spoken.empty()) {
      MarkupSegment segment;
      segment.type = MarkupSegment::Type::Speech;
      segment.text = spoken;
      segment.voice = activeVoice;
      result.segments.push_back(std::move(segment));
    }
    buffer.clear();
  };

  for (std::size_t i = 0; i < text.size();) {
    if (text[i] != '<') {
      buffer.push_back(text[i]);
      ++i;
      continue;
    }

    const auto end = text.find('>', i + 1);
    if (end == std::string::npos) {
      buffer.append(text.substr(i));
      break;
    }

    const auto rawTag = text.substr(i, end - i + 1);
    const auto lowerTag = lowerCopy(rawTag);
    if (lowerTag.rfind("<model", 0) == 0 && lowerTag.rfind("</model", 0) != 0) {
      flushSpeech();
      auto tagBody = rawTag.substr(1, rawTag.size() - 2);
      if (!tagBody.empty() && tagBody.back() == '/') {
        tagBody.pop_back();
      }
      activeVoice = parseMarkupModelSettings(parseLooseTagAttributes(tagBody));
      i = end + 1;
      continue;
    }

    if (lowerTag.rfind("</model", 0) == 0) {
      flushSpeech();
      activeVoice = MarkupVoiceSettings{};
      i = end + 1;
      continue;
    }

    if (lowerTag.rfind("<silence", 0) == 0) {
      flushSpeech();
      auto tagBody = rawTag.substr(1, rawTag.size() - 2);
      if (!tagBody.empty() && tagBody.back() == '/') {
        tagBody.pop_back();
      }
      const auto durationMs = parseSilenceDurationMs(parseLooseTagAttributes(tagBody));
      if (!durationMs) {
        throw std::runtime_error("markup_silence_missing_duration");
      }
      if (*durationMs > 0) {
        MarkupSegment segment;
        segment.type = MarkupSegment::Type::Silence;
        segment.silenceMs = *durationMs;
        result.segments.push_back(std::move(segment));
      }
      i = end + 1;
      continue;
    }

    buffer.append(rawTag);
    i = end + 1;
  }

  flushSpeech();
  return result;
}

std::optional<float> requestFloatOption(const json &input, const std::vector<std::string> &keys,
                                        const std::string &field, float minValue,
                                        float maxValue) {
  for (const auto &key : keys) {
    if (!input.contains(key)) {
      continue;
    }
    if (!input[key].is_number()) {
      throw std::runtime_error("invalid_request_" + field);
    }
    const auto value = input[key].get<float>();
    if (value < minValue || value > maxValue) {
      throw std::runtime_error("invalid_request_" + field);
    }
    return value;
  }
  return std::nullopt;
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

struct MarkupAudioPart {
  bool silence = false;
  std::uint64_t silenceMs = 0;
  std::string pcm;
  int sourceSampleRate = 0;
  std::string modelName;
  MarkupVoiceSettings settings;
  std::string text;
};

MarkupRenderResult synthesizeMarkupScript(const std::string &rawText, const std::string &fileName,
                                          const std::filesystem::path &outputPath,
                                          const std::optional<std::string> &defaultModel,
                                          const std::optional<piper::SpeakerId> &defaultSpeakerId,
                                          const std::optional<float> &defaultNoiseScale,
                                          const std::optional<float> &defaultLengthScale,
                                          const std::optional<float> &defaultNoiseW,
                                          const std::optional<float> &defaultSentenceSilence,
                                          const ServerOptions &options,
                                          FairTtsScheduler &scheduler,
                                          const std::function<bool()> &shouldCancel) {
  const auto parsed = parseMarkupScript(rawText);
  if (parsed.segments.empty()) {
    throw std::runtime_error("markup_empty_script");
  }

  const auto tempId = fileName.size() > 4 && fileName.substr(fileName.size() - 4) == ".wav"
                          ? fileName.substr(0, fileName.size() - 4)
                          : fileName;
  const auto tempDir = options.outputDir / "tmp" / (tempId + "_markup");
  std::filesystem::create_directories(tempDir);

  std::vector<MarkupAudioPart> parts;
  std::vector<json> pendingSegmentJson;
  int targetSampleRate = 0;
  int sampleWidth = 0;
  int channels = 0;
  piper::SynthesisResult totalSynthesis;
  std::size_t speechSegments = 0;
  std::size_t silenceSegments = 0;

  try {
    for (std::size_t i = 0; i < parsed.segments.size(); ++i) {
      const auto &segment = parsed.segments[i];
      if (shouldCancel && shouldCancel()) {
        throw std::runtime_error("synthesis_cancelled");
      }

      if (segment.type == MarkupSegment::Type::Silence) {
        MarkupAudioPart part;
        part.silence = true;
        part.silenceMs = segment.silenceMs;
        parts.push_back(std::move(part));
        ++silenceSegments;
        continue;
      }

      TtsTextSanitizeResult segmentSanitization;
      const auto maxSafeTextChars = std::max<std::size_t>(1000, std::min<std::size_t>(50000, options.maxInputBytes));
      const auto safeText = sanitizeTtsTextForApi(segment.text, maxSafeTextChars, segmentSanitization);
      if (!segmentSanitization.ok || safeText.empty()) {
        continue;
      }

      const auto segmentFileName = "segment_" + std::to_string(i) + ".wav";
      const auto segmentPath = tempDir / segmentFileName;
      TtsJobRequest jobRequest;
      jobRequest.text = safeText;
      jobRequest.fileName = segmentFileName;
      jobRequest.outputPath = segmentPath;
      jobRequest.requestedModel = segment.voice.model ? segment.voice.model : defaultModel;
      jobRequest.speakerId = segment.voice.speakerId ? segment.voice.speakerId : defaultSpeakerId;
      jobRequest.noiseScale = segment.voice.noiseScale ? segment.voice.noiseScale : defaultNoiseScale;
      jobRequest.lengthScale = segment.voice.lengthScale ? segment.voice.lengthScale : defaultLengthScale;
      jobRequest.noiseW = segment.voice.noiseW ? segment.voice.noiseW : defaultNoiseW;
      jobRequest.sentenceSilenceSeconds = segment.voice.sentenceSilenceSeconds ? segment.voice.sentenceSilenceSeconds : defaultSentenceSilence;
      jobRequest.shouldCancel = shouldCancel;

      auto ttsResult = scheduler.synthesize(jobRequest);
      auto payload = readServerWavPayload(segmentPath);
      if (payload.pcm.empty()) {
        continue;
      }

      if (targetSampleRate == 0) {
        targetSampleRate = payload.sampleRate;
        sampleWidth = payload.sampleWidth;
        channels = payload.channels;
      } else {
        targetSampleRate = std::max(targetSampleRate, payload.sampleRate);
        if (sampleWidth != payload.sampleWidth || channels != payload.channels) {
          throw std::runtime_error("markup_audio_format_mismatch");
        }
      }

      totalSynthesis.audioSeconds += ttsResult.synthesis.audioSeconds;
      totalSynthesis.inferSeconds += ttsResult.synthesis.inferSeconds;
      MarkupAudioPart part;
      part.silence = false;
      part.pcm = std::move(payload.pcm);
      part.sourceSampleRate = payload.sampleRate;
      part.modelName = ttsResult.modelName;
      part.settings = segment.voice;
      part.text = safeText;
      parts.push_back(std::move(part));
      ++speechSegments;
    }

    if (targetSampleRate == 0 || speechSegments == 0) {
      throw std::runtime_error("markup_empty_speech");
    }

    for (auto &part : parts) {
      if (!part.silence && part.sourceSampleRate != targetSampleRate) {
        part.pcm = resamplePcmS16Linear(part.pcm, part.sourceSampleRate, targetSampleRate, sampleWidth, channels);
        part.sourceSampleRate = targetSampleRate;
      }
    }

    std::uint64_t totalDataBytes = 0;
    for (const auto &part : parts) {
      if (part.silence) {
        totalDataBytes += silenceBytesForMs(part.silenceMs, targetSampleRate, sampleWidth, channels);
      } else {
        totalDataBytes += part.pcm.size();
      }
    }

    if (totalDataBytes > std::numeric_limits<std::uint32_t>::max()) {
      throw std::runtime_error("Generated WAV exceeds 4 GiB. Use smaller inputs or raw output.");
    }

    std::ofstream output(outputPath, std::ios::binary);
    if (!output.good()) {
      throw std::runtime_error("Could not open output file");
    }
    writeServerWavHeader(targetSampleRate, sampleWidth, channels,
                         static_cast<std::uint32_t>(totalDataBytes), output);

    std::uint64_t cursorMs = 0;
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
      } else {
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
    }
    output.close();

    totalSynthesis.audioSeconds = static_cast<double>(cursorMs) / 1000.0;
    if (totalSynthesis.audioSeconds > 0) {
      totalSynthesis.realTimeFactor = totalSynthesis.inferSeconds / totalSynthesis.audioSeconds;
    }

    MarkupRenderResult result;
    result.tts.fileName = fileName;
    result.tts.outputPath = outputPath;
    result.tts.modelName = defaultModel.value_or("<markup>");
    result.tts.chunks = speechSegments;
    result.tts.synthesis = totalSynthesis;
    result.tts.bytes = std::filesystem::exists(outputPath) ? std::filesystem::file_size(outputPath) : 0;
    result.segments = segmentsJson;
    result.speechSegments = speechSegments;
    result.silenceSegments = silenceSegments;
    result.outputSampleRate = targetSampleRate;

    std::error_code ignored;
    std::filesystem::remove_all(tempDir, ignored);
    return result;
  } catch (...) {
    std::error_code ignored;
    std::filesystem::remove_all(tempDir, ignored);
    throw;
  }
}

} // namespace piper_server
