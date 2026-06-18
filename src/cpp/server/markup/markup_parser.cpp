#include "server/markup/markup_parser.hpp"

#include <regex>
#include <stdexcept>
#include <vector>

#include "server/utils.hpp"

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
    const auto trimmed = trimCopy(value);
    const auto parsed = std::stof(trimmed, &used);
    if (used != trimmed.size() || parsed < minValue || parsed > maxValue) {
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

} // namespace piper_server
