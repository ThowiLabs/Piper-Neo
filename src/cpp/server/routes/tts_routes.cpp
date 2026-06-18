#include "server/routes/tts_routes.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>

#include <spdlog/spdlog.h>

#include "server/markup/request_options.hpp"
#include "server/markup_tts.hpp"
#include "server/responses.hpp"
#include "server/text_sanitizer.hpp"
#include "server/utils.hpp"

namespace piper_server {

namespace {

std::optional<std::string> requestedModelFromJson(SocketHandle clientSocket, const json &input,
                                                  bool &ok) {
  ok = true;
  std::optional<std::string> requestedModel;

  if (input.contains("model")) {
    if (!input["model"].is_string()) {
      sendJson(clientSocket, 400,
               errorResponse("invalid_request", "El campo model debe ser string."));
      ok = false;
      return std::nullopt;
    }
    requestedModel = input["model"].get<std::string>();
  }

  if (input.contains("default_model")) {
    if (!input["default_model"].is_string()) {
      sendJson(clientSocket, 400,
               errorResponse("invalid_request", "El campo default_model debe ser string."));
      ok = false;
      return std::nullopt;
    }
    requestedModel = input["default_model"].get<std::string>();
  }

  return requestedModel;
}

std::optional<piper::SpeakerId> speakerIdFromJson(SocketHandle clientSocket, const json &input,
                                                  bool &ok) {
  ok = true;
  if (!input.contains("speaker_id")) {
    return std::nullopt;
  }

  if (!input["speaker_id"].is_number_integer()) {
    sendJson(clientSocket, 400,
             errorResponse("invalid_request", "El campo speaker_id debe ser entero."));
    ok = false;
    return std::nullopt;
  }

  return input["speaker_id"].get<piper::SpeakerId>();
}

bool readSynthesisOptions(SocketHandle clientSocket, const json &input,
                          std::optional<float> &noiseScale,
                          std::optional<float> &lengthScale,
                          std::optional<float> &noiseW,
                          std::optional<float> &sentenceSilenceSeconds) {
  try {
    noiseScale = requestFloatOption(input, {"noise_scale", "noiseScale"}, "noise_scale", 0.0f, 2.0f);
    lengthScale = requestFloatOption(input, {"length_scale", "lengthScale"}, "length_scale", 0.05f, 10.0f);
    noiseW = requestFloatOption(input, {"noise_w", "noiseW"}, "noise_w", 0.0f, 2.0f);
    sentenceSilenceSeconds = requestFloatOption(input,
                                                {"sentence_silence", "sentenceSilence",
                                                 "sentence_silence_seconds"},
                                                "sentence_silence", 0.0f, 30.0f);
    return true;
  } catch (const std::runtime_error &e) {
    sendJson(clientSocket, 400, errorResponse("invalid_request", e.what()));
    return false;
  }
}

json ttsSuccessPayload(const std::string &fileName, const TtsJobResult &result,
                       const json &markup, const json &textPreprocessing,
                       const std::optional<std::string> &fallbackModel = std::nullopt) {
  return json{{"file", fileName},
              {"model", fallbackModel.value_or(result.modelName)},
              {"url", "/api/v1/files/" + fileName},
              {"format", "wav"},
              {"chunks", result.chunks},
              {"bytes", result.bytes},
              {"audio_seconds", result.synthesis.audioSeconds},
              {"infer_seconds", result.synthesis.inferSeconds},
              {"real_time_factor", result.synthesis.realTimeFactor},
              {"markup", markup},
              {"text_preprocessing", textPreprocessing}};
}

bool handleMarkupTts(SocketHandle clientSocket, const std::string &text,
                     const std::string &fileName, const std::filesystem::path &outputPath,
                     const std::optional<std::string> &requestedModel,
                     const std::optional<piper::SpeakerId> &speakerId,
                     const std::optional<float> &noiseScale,
                     const std::optional<float> &lengthScale,
                     const std::optional<float> &noiseW,
                     const std::optional<float> &sentenceSilenceSeconds,
                     const RouteContext &context,
                     const std::function<bool()> &shouldCancel) {
  auto result = synthesizeMarkupScript(text, fileName, outputPath, requestedModel, speakerId,
                                       noiseScale, lengthScale, noiseW, sentenceSilenceSeconds,
                                       context.options, context.scheduler, shouldCancel);
  context.metrics.sanitizedInputs++;

  sendJson(clientSocket, 201,
           successResponse("Audio generado exitosamente.",
                           ttsSuccessPayload(fileName, result.tts,
                                             json{{"enabled", true},
                                                  {"speech_segments", result.speechSegments},
                                                  {"silence_segments", result.silenceSegments},
                                                  {"sample_rate", result.outputSampleRate},
                                                  {"segments", result.segments}},
                                             json{{"mode", "markup"}},
                                             requestedModel.value_or(result.tts.modelName))));
  return true;
}

bool handlePlainTts(SocketHandle clientSocket, const std::string &text,
                    const std::string &fileName, const std::filesystem::path &outputPath,
                    const std::optional<std::string> &requestedModel,
                    const std::optional<piper::SpeakerId> &speakerId,
                    const std::optional<float> &noiseScale,
                    const std::optional<float> &lengthScale,
                    const std::optional<float> &noiseW,
                    const std::optional<float> &sentenceSilenceSeconds,
                    const RouteContext &context,
                    const std::function<bool()> &shouldCancel) {
  TtsTextSanitizeResult sanitization;
  const auto maxSafeTextChars = std::max<std::size_t>(
      1000, std::min<std::size_t>(50000, context.options.maxInputBytes));
  const auto safeText = sanitizeTtsTextForApi(text, maxSafeTextChars, sanitization);
  if (!sanitization.ok || safeText.empty()) {
    context.metrics.rejectedTextInputs++;
    sendJson(clientSocket, sanitization.warnings.empty() ? 400 : 422,
             errorResponse("text_not_pronounceable",
                           "El texto no contiene contenido pronunciable seguro después del filtrado."));
    return true;
  }

  context.metrics.sanitizedInputs++;
  context.metrics.sanitizeWarnings.fetch_add(static_cast<std::uint64_t>(sanitization.warnings.size()));
  if (!sanitization.warnings.empty()) {
    spdlog::info("{} TTS text sanitized: raw_bytes={} speak_bytes={} warnings={} risk={}",
                 nowIso8601(), sanitization.rawBytes, sanitization.speakBytes,
                 sanitization.warnings.size(), sanitization.riskScore);
  }

  TtsJobRequest jobRequest;
  jobRequest.text = safeText;
  jobRequest.fileName = fileName;
  jobRequest.outputPath = outputPath;
  jobRequest.requestedModel = requestedModel;
  jobRequest.speakerId = speakerId;
  jobRequest.noiseScale = noiseScale;
  jobRequest.lengthScale = lengthScale;
  jobRequest.noiseW = noiseW;
  jobRequest.sentenceSilenceSeconds = sentenceSilenceSeconds;
  jobRequest.shouldCancel = shouldCancel;

  auto result = context.scheduler.synthesize(jobRequest);
  sendJson(clientSocket, 201,
           successResponse("Audio generado exitosamente.",
                           ttsSuccessPayload(fileName, result,
                                             json{{"enabled", false}},
                                             ttsSanitizeResultToJson(sanitization))));
  return true;
}

} // namespace

bool handleTtsRoute(SocketHandle clientSocket, const HttpRequest &request,
                    const ParsedTarget &target, const RouteContext &context) {
  if (target.path != "/api/v1/tts") {
    return false;
  }

  if (request.method != "POST") {
    sendJson(clientSocket, 405,
             errorResponse("method_not_allowed", "Método no permitido."));
    return true;
  }

  json input;
  try {
    input = json::parse(request.body);
  } catch (const std::exception &) {
    sendJson(clientSocket, 400,
             errorResponse("invalid_json", "El body debe ser JSON válido."));
    return true;
  }

  if (!input.contains("text") || !input["text"].is_string() ||
      input["text"].get<std::string>().empty()) {
    sendJson(clientSocket, 400,
             errorResponse("missing_fields", "El campo text es obligatorio."));
    return true;
  }

  const auto text = input["text"].get<std::string>();
  if (text.size() > context.options.maxInputBytes) {
    sendJson(clientSocket, 413,
             errorResponse("payload_too_large", "El texto excede el límite permitido."));
    return true;
  }

  if (input.contains("output_file") || input.contains("outputFile")) {
    sendJson(clientSocket, 400,
             errorResponse("invalid_request",
                           "output_file ya no está soportado. Piper Neo genera nombres seguros automáticamente."));
    return true;
  }

  bool ok = true;
  auto requestedModel = requestedModelFromJson(clientSocket, input, ok);
  if (!ok) {
    return true;
  }

  auto speakerId = speakerIdFromJson(clientSocket, input, ok);
  if (!ok) {
    return true;
  }

  std::optional<float> noiseScale;
  std::optional<float> lengthScale;
  std::optional<float> noiseW;
  std::optional<float> sentenceSilenceSeconds;
  if (!readSynthesisOptions(clientSocket, input, noiseScale, lengthScale, noiseW,
                            sentenceSilenceSeconds)) {
    return true;
  }

  std::filesystem::create_directories(context.options.outputDir);
  std::string fileName = makeOutputFileName();
  const auto outputPath = context.options.outputDir / fileName;
  auto shouldCancel = [&clientSocket]() { return clientDisconnected(clientSocket); };

  try {
    if (looksLikeMarkupTts(text)) {
      return handleMarkupTts(clientSocket, text, fileName, outputPath, requestedModel, speakerId,
                             noiseScale, lengthScale, noiseW, sentenceSilenceSeconds, context,
                             shouldCancel);
    }

    return handlePlainTts(clientSocket, text, fileName, outputPath, requestedModel, speakerId,
                          noiseScale, lengthScale, noiseW, sentenceSilenceSeconds, context,
                          shouldCancel);
  } catch (const std::runtime_error &e) {
    const std::string message = e.what();
    if (message == "synthesis_cancelled") {
      spdlog::warn("TTS request cancelled because client disconnected");
      std::error_code ignored;
      std::filesystem::remove(outputPath, ignored);
      return true;
    }

    std::error_code ignored;
    std::filesystem::remove(outputPath, ignored);
    sendJson(clientSocket, errorStatusForException(message), modelErrorResponse(message));
  } catch (const std::exception &e) {
    std::error_code ignored;
    std::filesystem::remove(outputPath, ignored);
    sendJson(clientSocket, 500, errorResponse("synthesis_error", e.what()));
  }

  return true;
}

} // namespace piper_server
