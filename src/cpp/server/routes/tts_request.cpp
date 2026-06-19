#include "server/routes/tts_request.hpp"

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string>

#include "server/markup/request_options.hpp"

namespace piper_server {
namespace {

json errorJson(const std::string &code, const std::string &message) {
  return json{{"success", false}, {"error", code}, {"message", message}};
}

TtsRouteParseResult failRequest(int status, const json &response) {
  TtsRouteParseResult result;
  result.ok = false;
  result.status = status;
  result.response = response;
  return result;
}

std::optional<std::string> requestedModelFromJson(const json &input,
                                                  TtsRouteParseResult &result) {
  std::optional<std::string> requestedModel;

  if (input.contains("model")) {
    if (!input["model"].is_string()) {
      result = failRequest(400, errorJson("invalid_request", "El campo model debe ser string."));
      return std::nullopt;
    }
    requestedModel = input["model"].get<std::string>();
  }

  if (input.contains("default_model")) {
    if (!input["default_model"].is_string()) {
      result = failRequest(400, errorJson("invalid_request", "El campo default_model debe ser string."));
      return std::nullopt;
    }
    requestedModel = input["default_model"].get<std::string>();
  }

  return requestedModel;
}

std::optional<std::int64_t> speakerIdFromJson(const json &input,
                                                  TtsRouteParseResult &result) {
  if (!input.contains("speaker_id")) {
    return std::nullopt;
  }

  if (!input["speaker_id"].is_number_integer()) {
    result = failRequest(400, errorJson("invalid_request", "El campo speaker_id debe ser entero."));
    return std::nullopt;
  }

  return input["speaker_id"].get<std::int64_t>();
}

bool readSynthesisOptions(const json &input, TtsRouteRequest &routeRequest,
                          TtsRouteParseResult &result) {
  try {
    routeRequest.noiseScale = requestFloatOption(input, {"noise_scale", "noiseScale"},
                                                 "noise_scale", 0.0f, 2.0f);
    routeRequest.lengthScale = requestFloatOption(input, {"length_scale", "lengthScale"},
                                                  "length_scale", 0.05f, 10.0f);
    routeRequest.noiseW = requestFloatOption(input, {"noise_w", "noiseW"},
                                             "noise_w", 0.0f, 2.0f);
    routeRequest.sentenceSilenceSeconds = requestFloatOption(
        input, {"sentence_silence", "sentenceSilence", "sentence_silence_seconds"},
        "sentence_silence", 0.0f, 30.0f);
    return true;
  } catch (const std::runtime_error &e) {
    result = failRequest(400, errorJson("invalid_request", e.what()));
    return false;
  }
}

} // namespace

TtsRouteParseResult parseTtsRouteRequest(const HttpRequest &request,
                                         std::size_t maxInputBytes) {
  json input;
  try {
    input = json::parse(request.body);
  } catch (const std::exception &) {
    return failRequest(400, errorJson("invalid_json", "El body debe ser JSON válido."));
  }

  if (!input.contains("text") || !input["text"].is_string() ||
      input["text"].get<std::string>().empty()) {
    return failRequest(400, errorJson("missing_fields", "El campo text es obligatorio."));
  }

  TtsRouteParseResult result;
  result.value.text = input["text"].get<std::string>();
  if (result.value.text.size() > maxInputBytes) {
    return failRequest(413, errorJson("payload_too_large", "El texto excede el límite permitido."));
  }

  if (input.contains("output_file") || input.contains("outputFile")) {
    return failRequest(400, errorJson("invalid_request",
                                          "output_file ya no está soportado. Piper Neo genera nombres seguros automáticamente."));
  }

  result.value.requestedModel = requestedModelFromJson(input, result);
  if (!result.response.empty()) {
    return result;
  }

  result.value.speakerId = speakerIdFromJson(input, result);
  if (!result.response.empty()) {
    return result;
  }

  if (!readSynthesisOptions(input, result.value, result)) {
    return result;
  }

  result.ok = true;
  result.status = 200;
  return result;
}

} // namespace piper_server
