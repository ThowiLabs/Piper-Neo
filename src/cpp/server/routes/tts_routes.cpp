#include "server/routes/tts_routes.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>

#include <spdlog/spdlog.h>

#include "server/markup_tts.hpp"
#include "server/responses.hpp"
#include "server/routes/tts_payload.hpp"
#include "server/routes/tts_request.hpp"
#include "server/text_sanitizer.hpp"
#include "server/utils.hpp"

namespace piper_server {
namespace {

bool handleMarkupTts(SocketHandle clientSocket, const TtsRouteRequest &routeRequest,
                     const std::string &fileName,
                     const std::filesystem::path &outputPath,
                     const RouteContext &context,
                     const std::function<bool()> &shouldCancel) {
  auto result = synthesizeMarkupScript(
      routeRequest.text, fileName, outputPath, routeRequest.requestedModel,
      routeRequest.speakerId, routeRequest.noiseScale, routeRequest.lengthScale,
      routeRequest.noiseW, routeRequest.sentenceSilenceSeconds, context.options,
      context.scheduler, shouldCancel);
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
                                             routeRequest.requestedModel.value_or(result.tts.modelName))));
  return true;
}

bool handlePlainTts(SocketHandle clientSocket, const TtsRouteRequest &routeRequest,
                    const std::string &fileName,
                    const std::filesystem::path &outputPath,
                    const RouteContext &context,
                    const std::function<bool()> &shouldCancel) {
  TtsTextSanitizeResult sanitization;
  const auto maxSafeTextChars = std::max<std::size_t>(
      1000, std::min<std::size_t>(50000, context.options.maxInputBytes));
  const auto safeText = sanitizeTtsTextForApi(routeRequest.text, maxSafeTextChars, sanitization);
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
  jobRequest.requestedModel = routeRequest.requestedModel;
  jobRequest.speakerId = routeRequest.speakerId;
  jobRequest.noiseScale = routeRequest.noiseScale;
  jobRequest.lengthScale = routeRequest.lengthScale;
  jobRequest.noiseW = routeRequest.noiseW;
  jobRequest.sentenceSilenceSeconds = routeRequest.sentenceSilenceSeconds;
  jobRequest.shouldCancel = shouldCancel;

  auto result = context.scheduler.synthesize(jobRequest);
  sendJson(clientSocket, 201,
           successResponse("Audio generado exitosamente.",
                           ttsSuccessPayload(fileName, result,
                                             json{{"enabled", false}},
                                             ttsSanitizeResultToJson(sanitization))));
  return true;
}

void removePartialOutput(const std::filesystem::path &outputPath) {
  std::error_code ignored;
  std::filesystem::remove(outputPath, ignored);
}

bool synthesizeTtsRequest(SocketHandle clientSocket, const TtsRouteRequest &routeRequest,
                          const RouteContext &context) {
  std::filesystem::create_directories(context.options.outputDir);
  std::string fileName = makeOutputFileName();
  const auto outputPath = context.options.outputDir / fileName;
  auto shouldCancel = [&clientSocket]() { return clientDisconnected(clientSocket); };

  try {
    if (looksLikeMarkupTts(routeRequest.text)) {
      return handleMarkupTts(clientSocket, routeRequest, fileName, outputPath,
                             context, shouldCancel);
    }

    return handlePlainTts(clientSocket, routeRequest, fileName, outputPath,
                          context, shouldCancel);
  } catch (const std::runtime_error &e) {
    const std::string message = e.what();
    if (message == "synthesis_cancelled") {
      spdlog::warn("TTS request cancelled because client disconnected");
      removePartialOutput(outputPath);
      return true;
    }

    removePartialOutput(outputPath);
    sendJson(clientSocket, errorStatusForException(message), modelErrorResponse(message));
  } catch (const std::exception &e) {
    removePartialOutput(outputPath);
    sendJson(clientSocket, 500, errorResponse("synthesis_error", e.what()));
  }

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

  auto parsed = parseTtsRouteRequest(request, context.options.maxInputBytes);
  if (!parsed.ok) {
    sendJson(clientSocket, parsed.status, parsed.response);
    return true;
  }

  return synthesizeTtsRequest(clientSocket, parsed.value, context);
}

} // namespace piper_server
