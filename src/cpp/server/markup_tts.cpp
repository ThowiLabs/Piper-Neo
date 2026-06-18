#include "markup_tts.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "markup/audio_parts.hpp"
#include "text_sanitizer.hpp"
#include "utils.hpp"
#include "wav_utils.hpp"

namespace piper_server {
namespace {

std::filesystem::path markupTempDirFor(const ServerOptions &options, const std::string &fileName) {
  const auto tempId = fileName.size() > 4 && fileName.substr(fileName.size() - 4) == ".wav"
                          ? fileName.substr(0, fileName.size() - 4)
                          : fileName;
  return options.outputDir / "tmp" / (tempId + "_markup");
}

std::string sanitizeMarkupSpeechText(const std::string &text, const ServerOptions &options) {
  TtsTextSanitizeResult segmentSanitization;
  const auto maxSafeTextChars = std::max<std::size_t>(1000, std::min<std::size_t>(50000, options.maxInputBytes));
  const auto safeText = sanitizeTtsTextForApi(text, maxSafeTextChars, segmentSanitization);
  if (!segmentSanitization.ok || safeText.empty()) {
    return {};
  }
  return safeText;
}

TtsJobRequest makeMarkupJobRequest(const MarkupSegment &segment, const std::string &safeText,
                                   const std::filesystem::path &segmentPath,
                                   const std::string &segmentFileName,
                                   const std::optional<std::string> &defaultModel,
                                   const std::optional<piper::SpeakerId> &defaultSpeakerId,
                                   const std::optional<float> &defaultNoiseScale,
                                   const std::optional<float> &defaultLengthScale,
                                   const std::optional<float> &defaultNoiseW,
                                   const std::optional<float> &defaultSentenceSilence,
                                   const std::function<bool()> &shouldCancel) {
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
  return jobRequest;
}

void updateOutputFormatFromPayload(const WavPayload &payload, int &targetSampleRate,
                                   int &sampleWidth, int &channels) {
  if (targetSampleRate == 0) {
    targetSampleRate = payload.sampleRate;
    sampleWidth = payload.sampleWidth;
    channels = payload.channels;
    return;
  }

  targetSampleRate = std::max(targetSampleRate, payload.sampleRate);
  if (sampleWidth != payload.sampleWidth || channels != payload.channels) {
    throw std::runtime_error("markup_audio_format_mismatch");
  }
}

MarkupAudioPart silencePartFor(const MarkupSegment &segment) {
  MarkupAudioPart part;
  part.silence = true;
  part.silenceMs = segment.silenceMs;
  return part;
}

MarkupAudioPart speechPartFor(const MarkupSegment &segment, const std::string &safeText,
                              TtsJobResult &ttsResult, WavPayload &payload) {
  MarkupAudioPart part;
  part.silence = false;
  part.pcm = std::move(payload.pcm);
  part.sourceSampleRate = payload.sampleRate;
  part.modelName = ttsResult.modelName;
  part.settings = segment.voice;
  part.text = safeText;
  return part;
}

void writeMarkupOutputFile(const std::filesystem::path &outputPath, const std::vector<MarkupAudioPart> &parts,
                           int targetSampleRate, int sampleWidth, int channels,
                           const std::optional<std::string> &defaultModel,
                           const std::optional<piper::SpeakerId> &defaultSpeakerId,
                           std::uint64_t &cursorMs, json &segmentsJson) {
  const auto totalDataBytes = markupAudioDataBytes(parts, targetSampleRate, sampleWidth, channels);
  if (totalDataBytes > std::numeric_limits<std::uint32_t>::max()) {
    throw std::runtime_error("Generated WAV exceeds 4 GiB. Use smaller inputs or raw output.");
  }

  std::ofstream output(outputPath, std::ios::binary);
  if (!output.good()) {
    throw std::runtime_error("Could not open output file");
  }

  writeServerWavHeader(targetSampleRate, sampleWidth, channels,
                       static_cast<std::uint32_t>(totalDataBytes), output);
  segmentsJson = writeMarkupAudioParts(output, parts, defaultModel, defaultSpeakerId,
                                       targetSampleRate, sampleWidth, channels, cursorMs);
  output.close();
  if (!output.good()) {
    throw std::runtime_error("Could not write output file");
  }
}

} // namespace

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

  const auto tempDir = markupTempDirFor(options, fileName);
  std::filesystem::create_directories(tempDir);

  std::vector<MarkupAudioPart> parts;
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
        parts.push_back(silencePartFor(segment));
        ++silenceSegments;
        continue;
      }

      const auto safeText = sanitizeMarkupSpeechText(segment.text, options);
      if (safeText.empty()) {
        continue;
      }

      const auto segmentFileName = "segment_" + std::to_string(i) + ".wav";
      const auto segmentPath = tempDir / segmentFileName;
      auto jobRequest = makeMarkupJobRequest(segment, safeText, segmentPath, segmentFileName,
                                             defaultModel, defaultSpeakerId, defaultNoiseScale,
                                             defaultLengthScale, defaultNoiseW,
                                             defaultSentenceSilence, shouldCancel);

      auto ttsResult = scheduler.synthesize(jobRequest);
      auto payload = readServerWavPayload(segmentPath);
      if (payload.pcm.empty()) {
        continue;
      }

      updateOutputFormatFromPayload(payload, targetSampleRate, sampleWidth, channels);
      totalSynthesis.audioSeconds += ttsResult.synthesis.audioSeconds;
      totalSynthesis.inferSeconds += ttsResult.synthesis.inferSeconds;
      parts.push_back(speechPartFor(segment, safeText, ttsResult, payload));
      ++speechSegments;
    }

    if (targetSampleRate == 0 || speechSegments == 0) {
      throw std::runtime_error("markup_empty_speech");
    }

    normalizeMarkupAudioSampleRates(parts, targetSampleRate, sampleWidth, channels);

    std::uint64_t cursorMs = 0;
    json segmentsJson = json::array();
    writeMarkupOutputFile(outputPath, parts, targetSampleRate, sampleWidth, channels,
                          defaultModel, defaultSpeakerId, cursorMs, segmentsJson);

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
