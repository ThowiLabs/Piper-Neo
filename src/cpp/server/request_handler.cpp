#include "request_handler.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>

#include <spdlog/spdlog.h>

#include "../neo_model.hpp"
#include "auth.hpp"
#include "http.hpp"
#include "markup_tts.hpp"
#include "metrics_report.hpp"
#include "model_cache.hpp"
#include "model_registry.hpp"
#include "responses.hpp"
#include "text_sanitizer.hpp"
#include "tts_scheduler.hpp"
#include "utils.hpp"

namespace piper_server {

void handleClient(SocketHandle clientSocket, const ServerOptions &options, ModelCache &modelCache,
                  ModelRegistry &modelRegistry, FairTtsScheduler &scheduler,
                  ServerMetrics &metrics) {
  try {
    auto maybeRequest = readHttpRequest(clientSocket, options.maxInputBytes);
    if (!maybeRequest) {
      closeSocket(clientSocket);
      return;
    }

    const auto &request = *maybeRequest;
    const auto target = parseTarget(request.path);

    if (request.method == "OPTIONS") {
      sendResponse(clientSocket, 200, "text/plain; charset=utf-8", "");
      closeSocket(clientSocket);
      return;
    }

    if (!requestIsAuthorized(request, options)) {
      sendJson(clientSocket, 401,
               errorResponse("invalid_token",
                             "Token inválido, ausente o expirado."));
      closeSocket(clientSocket);
      return;
    }

    if ((request.method == "GET") && (target.path == "/api/health")) {
      sendJson(clientSocket, 200,
               successResponse("Servidor Piper activo.",
                               json{{"status", "ok"},
                                    {"model_loaded", true},
                                    {"time", nowIso8601()}}));
      closeSocket(clientSocket);
      return;
    }

    if ((request.method == "GET") && (target.path == "/api/v1/status")) {
      sendJson(clientSocket, 200,
               successResponse("Estado obtenido correctamente.",
                               json{{"server", "piper-neo"},
                                    {"model_loaded", true},
                                    {"active_model", options.activeModelPath.filename().string()},
                                    {"models_dir", options.modelsDir.filename().string()},
                                    {"output_dir", options.outputDir.filename().string()},
                                    {"auth",
                                     json{{"enabled", !options.apiToken.empty()},
                                          {"header", "Authorization: Bearer <token>"}}},
                                    {"limits",
                                     json{{"max_input_bytes", options.maxInputBytes},
                                          {"max_text_chunk_bytes", options.maxTextChunkBytes},
                                          {"text_sanitizer_enabled", true},
                                          {"max_sanitized_text_chars", std::max<std::size_t>(1000, std::min<std::size_t>(50000, options.maxInputBytes))},
                                          {"max_temp_bytes", options.maxTempBytes},
                                          {"output_retention_seconds", options.outputRetentionSeconds},
                                          {"models_refresh_seconds", options.modelsRefreshSeconds}}},
                                    {"resource_policy", resourcePolicyJson(options, scheduler)}}));
      closeSocket(clientSocket);
      return;
    }

    if ((request.method == "GET") && (target.path == "/api/v1/metrics")) {
      sendJson(clientSocket, 200,
               successResponse("Métricas obtenidas correctamente.",
                               metricsJson(metrics, scheduler, options)));
      closeSocket(clientSocket);
      return;
    }

    if ((request.method == "GET") && (target.path == "/api/v1/models")) {
      auto includeMode = queryValue(target, "include").value_or("basic");
      includeMode = lowerCopy(includeMode);
      if (includeMode != "basic" && includeMode != "metadata" && includeMode != "technical") {
        sendJson(clientSocket, 400,
                 errorResponse("invalid_request", "include debe ser basic, metadata o technical."));
        closeSocket(clientSocket);
        return;
      }

      json models = modelRegistry.list(includeMode);
      sendJson(clientSocket, 200,
               successResponse("Modelos listados correctamente.",
                               json{{"total", models.size()},
                                    {"include", includeMode},
                                    {"cached", true},
                                    {"refresh_seconds", options.modelsRefreshSeconds},
                                    {"models", models}}));
      closeSocket(clientSocket);
      return;
    }

    if (auto imageModelName = routeModelImageName(target.path)) {
      if (request.method != "GET") {
        sendJson(clientSocket, 405,
                 errorResponse("method_not_allowed", "Método no permitido."));
        closeSocket(clientSocket);
        return;
      }

      if (!isSafeFileName(*imageModelName)) {
        sendJson(clientSocket, 400,
                 errorResponse("invalid_request", "Nombre de modelo inválido."));
        closeSocket(clientSocket);
        return;
      }

      try {
        auto modelInfo = findModelByName(options, *imageModelName, &modelRegistry);
        if (!modelInfo || !modelInfo->hasConfig) {
          sendJson(clientSocket, 404,
                   errorResponse("not_found", "Modelo o configuración no encontrada."));
          closeSocket(clientSocket);
          return;
        }
        std::string contentType;
        std::string imageBytes;
        if (modelInfo->isNeo) {
          auto neoImage = piper_neo::readImageSection(modelInfo->modelPath);
          contentType = neoImage.first;
          imageBytes = neoImage.second;
        } else {
          auto root = loadJsonFile(modelInfo->configPath);
          if (!modelJsonHasImage(root)) {
            sendJson(clientSocket, 404,
                     errorResponse("not_found", "El modelo no tiene imagen."));
            closeSocket(clientSocket);
            return;
          }
          auto parsedImage = parseDataImage(root["modelcard"]["image"].get<std::string>());
          contentType = parsedImage.first;
          imageBytes = parsedImage.second;
        }
        sendResponse(clientSocket, 200, contentType, imageBytes,
                     {{"Cache-Control", "public, max-age=3600"}});
      } catch (const std::runtime_error &e) {
        const std::string message = e.what();
        if (message == "invalid_image") {
          sendJson(clientSocket, 400,
                   errorResponse("invalid_image", "La imagen del modelo no tiene un formato válido."));
        } else {
          sendJson(clientSocket, 500, errorResponse("server_error", message));
        }
      }
      closeSocket(clientSocket);
      return;
    }

    if (auto fileName = routeFileName(target.path)) {
      if (request.method != "GET") {
        sendJson(clientSocket, 405,
                 errorResponse("method_not_allowed", "Método no permitido."));
        closeSocket(clientSocket);
        return;
      }

      if (!isSafeFileName(*fileName)) {
        sendJson(clientSocket, 400,
                 errorResponse("invalid_request", "Nombre de archivo inválido."));
        closeSocket(clientSocket);
        return;
      }

      auto filePath = options.outputDir / *fileName;
      sendFile(clientSocket, filePath);
      closeSocket(clientSocket);
      return;
    }

    if ((request.method == "POST") && (target.path == "/api/v1/tts")) {
      json input;
      try {
        input = json::parse(request.body);
      } catch (const std::exception &) {
        sendJson(clientSocket, 400,
                 errorResponse("invalid_json", "El body debe ser JSON válido."));
        closeSocket(clientSocket);
        return;
      }

      if (!input.contains("text") || !input["text"].is_string() ||
          input["text"].get<std::string>().empty()) {
        sendJson(clientSocket, 400,
                 errorResponse("missing_fields", "El campo text es obligatorio."));
        closeSocket(clientSocket);
        return;
      }

      const auto text = input["text"].get<std::string>();
      if (text.size() > options.maxInputBytes) {
        sendJson(clientSocket, 413,
                 errorResponse("payload_too_large",
                               "El texto excede el límite permitido."));
        closeSocket(clientSocket);
        return;
      }

      if (input.contains("output_file") || input.contains("outputFile")) {
        sendJson(clientSocket, 400,
                 errorResponse("invalid_request",
                               "output_file ya no está soportado. Piper Neo genera nombres seguros automáticamente."));
        closeSocket(clientSocket);
        return;
      }

      std::string fileName = makeOutputFileName();

      std::optional<std::string> requestedModel;
      if (input.contains("model")) {
        if (!input["model"].is_string()) {
          sendJson(clientSocket, 400,
                   errorResponse("invalid_request", "El campo model debe ser string."));
          closeSocket(clientSocket);
          return;
        }
        requestedModel = input["model"].get<std::string>();
      }
      if (input.contains("default_model")) {
        if (!input["default_model"].is_string()) {
          sendJson(clientSocket, 400,
                   errorResponse("invalid_request", "El campo default_model debe ser string."));
          closeSocket(clientSocket);
          return;
        }
        requestedModel = input["default_model"].get<std::string>();
      }

      std::optional<piper::SpeakerId> speakerId;
      if (input.contains("speaker_id")) {
        if (!input["speaker_id"].is_number_integer()) {
          sendJson(clientSocket, 400,
                   errorResponse("invalid_request", "El campo speaker_id debe ser entero."));
          closeSocket(clientSocket);
          return;
        }
        speakerId = input["speaker_id"].get<piper::SpeakerId>();
      }

      std::optional<float> noiseScale;
      std::optional<float> lengthScale;
      std::optional<float> noiseW;
      std::optional<float> sentenceSilenceSeconds;
      try {
        noiseScale = requestFloatOption(input, {"noise_scale", "noiseScale"}, "noise_scale", 0.0f, 2.0f);
        lengthScale = requestFloatOption(input, {"length_scale", "lengthScale"}, "length_scale", 0.05f, 10.0f);
        noiseW = requestFloatOption(input, {"noise_w", "noiseW"}, "noise_w", 0.0f, 2.0f);
        sentenceSilenceSeconds = requestFloatOption(input, {"sentence_silence", "sentenceSilence", "sentence_silence_seconds"},
                                                    "sentence_silence", 0.0f, 30.0f);
      } catch (const std::runtime_error &e) {
        sendJson(clientSocket, 400,
                 errorResponse("invalid_request", e.what()));
        closeSocket(clientSocket);
        return;
      }

      std::filesystem::create_directories(options.outputDir);
      const auto outputPath = options.outputDir / fileName;
      auto shouldCancel = [&clientSocket]() {
        return clientDisconnected(clientSocket);
      };

      try {
        if (looksLikeMarkupTts(text)) {
          auto result = synthesizeMarkupScript(text, fileName, outputPath, requestedModel, speakerId,
                                               noiseScale, lengthScale, noiseW, sentenceSilenceSeconds,
                                               options, scheduler, shouldCancel);
          metrics.sanitizedInputs++;

          sendJson(clientSocket, 201,
                   successResponse("Audio generado exitosamente.",
                                   json{{"file", fileName},
                                        {"model", requestedModel.value_or(result.tts.modelName)},
                                        {"url", "/api/v1/files/" + fileName},
                                        {"format", "wav"},
                                        {"chunks", result.tts.chunks},
                                        {"bytes", result.tts.bytes},
                                        {"audio_seconds", result.tts.synthesis.audioSeconds},
                                        {"infer_seconds", result.tts.synthesis.inferSeconds},
                                        {"real_time_factor", result.tts.synthesis.realTimeFactor},
                                        {"markup", json{{"enabled", true},
                                                         {"speech_segments", result.speechSegments},
                                                         {"silence_segments", result.silenceSegments},
                                                         {"sample_rate", result.outputSampleRate},
                                                         {"segments", result.segments}}},
                                        {"text_preprocessing", json{{"mode", "markup"}}}}));
        } else {
          TtsTextSanitizeResult sanitization;
          const auto maxSafeTextChars = std::max<std::size_t>(1000, std::min<std::size_t>(50000, options.maxInputBytes));
          const auto safeText = sanitizeTtsTextForApi(text, maxSafeTextChars, sanitization);
          if (!sanitization.ok || safeText.empty()) {
            metrics.rejectedTextInputs++;
            sendJson(clientSocket, sanitization.warnings.empty() ? 400 : 422,
                     errorResponse("text_not_pronounceable",
                                   "El texto no contiene contenido pronunciable seguro después del filtrado."));
            closeSocket(clientSocket);
            return;
          }

          metrics.sanitizedInputs++;
          metrics.sanitizeWarnings.fetch_add(static_cast<std::uint64_t>(sanitization.warnings.size()));
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

          auto result = scheduler.synthesize(jobRequest);

          sendJson(clientSocket, 201,
                   successResponse("Audio generado exitosamente.",
                                   json{{"file", fileName},
                                        {"model", result.modelName},
                                        {"url", "/api/v1/files/" + fileName},
                                        {"format", "wav"},
                                        {"chunks", result.chunks},
                                        {"bytes", result.bytes},
                                        {"audio_seconds", result.synthesis.audioSeconds},
                                        {"infer_seconds", result.synthesis.inferSeconds},
                                        {"real_time_factor", result.synthesis.realTimeFactor},
                                        {"markup", json{{"enabled", false}}},
                                        {"text_preprocessing", ttsSanitizeResultToJson(sanitization)}}));
        }
      } catch (const std::runtime_error &e) {
        const std::string message = e.what();
        if (message == "synthesis_cancelled") {
          spdlog::warn("TTS request cancelled because client disconnected");
          std::error_code ignored;
          std::filesystem::remove(outputPath, ignored);
          closeSocket(clientSocket);
          return;
        }

        std::error_code ignored;
        std::filesystem::remove(outputPath, ignored);
        sendJson(clientSocket, errorStatusForException(message),
                 modelErrorResponse(message));
      } catch (const std::exception &e) {
        std::error_code ignored;
        std::filesystem::remove(outputPath, ignored);
        sendJson(clientSocket, 500,
                 errorResponse("synthesis_error", e.what()));
      }

      closeSocket(clientSocket);
      return;
    }

    sendJson(clientSocket, 404,
             errorResponse("not_found", "Endpoint no encontrado."));
  } catch (const std::runtime_error &e) {
    const std::string message = e.what();
    if (message == "payload_too_large") {
      sendJson(clientSocket, 413,
               errorResponse("payload_too_large",
                             "El payload excede el límite permitido."));
    } else {
      sendJson(clientSocket, 500,
               errorResponse("server_error", message));
    }
  } catch (const std::exception &e) {
    sendJson(clientSocket, 500,
             errorResponse("server_error", e.what()));
  }

  closeSocket(clientSocket);
}

} // namespace piper_server
