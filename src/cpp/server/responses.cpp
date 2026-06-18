#include "responses.hpp"

#include <string>

namespace piper_server {

json successResponse(const std::string &message, const json &data) {
  json response;
  response["success"] = true;
  if (!message.empty()) {
    response["message"] = message;
  }
  response["data"] = data;
  return response;
}

json errorResponse(const std::string &error, const std::string &message) {
  json response;
  response["success"] = false;
  response["error"] = error;
  response["message"] = message;
  return response;
}

int errorStatusForException(const std::string &error) {
  if (error == "invalid_model" || error.rfind("markup_", 0) == 0 || error.rfind("invalid_request_", 0) == 0) {
    return 400;
  }
  if (error == "model_not_found" || error == "model_config_missing") {
    return 404;
  }
  if (error == "model_busy" || error == "server_busy") {
    return 429;
  }
  if (error == "temp_storage_full") {
    return 507;
  }
  return 500;
}

json modelErrorResponse(const std::string &error) {
  if (error == "invalid_model") {
    return errorResponse("invalid_request",
                         "El nombre del modelo es inválido. Usa solo el nombre del archivo .onnx o .neo dentro de models/.");
  }
  if (error.rfind("markup_", 0) == 0) {
    return errorResponse("invalid_markup", error);
  }
  if (error.rfind("invalid_request_", 0) == 0) {
    return errorResponse("invalid_request", error);
  }
  if (error == "model_not_found") {
    return errorResponse("model_not_found", "Modelo no encontrado en la carpeta models/.");
  }
  if (error == "model_config_missing") {
    return errorResponse("model_config_missing",
                         "El modelo existe, pero falta su archivo .onnx.json.");
  }
  if (error == "model_busy") {
    return errorResponse("server_busy",
                         "Todas las réplicas de ese modelo están ocupadas. Intenta nuevamente.");
  }
  if (error == "server_busy") {
    return errorResponse("server_busy",
                         "El servidor alcanzó el límite de síntesis simultáneas. Intenta nuevamente.");
  }
  if (error == "temp_storage_full") {
    return errorResponse("temp_storage_full",
                         "El servidor alcanzó el límite de almacenamiento temporal para síntesis.");
  }
  return errorResponse("server_error", error);
}

} // namespace piper_server
