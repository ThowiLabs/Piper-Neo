#include "server/routes/model_routes.hpp"

#include <stdexcept>
#include <string>

#include "neo_model.hpp"
#include "server/model_registry.hpp"
#include "server/media/data_image.hpp"
#include "server/responses.hpp"
#include "server/utils.hpp"

namespace piper_server {

namespace {

void sendMethodNotAllowed(SocketHandle clientSocket) {
  sendJson(clientSocket, 405,
           errorResponse("method_not_allowed", "Método no permitido."));
}

} // namespace

bool handleModelRoute(SocketHandle clientSocket, const HttpRequest &request,
                      const ParsedTarget &target, const RouteContext &context) {
  if ((request.method == "GET") && (target.path == "/api/v1/models")) {
    auto includeMode = queryValue(target, "include").value_or("basic");
    includeMode = lowerCopy(includeMode);
    if (includeMode != "basic" && includeMode != "metadata" && includeMode != "technical") {
      sendJson(clientSocket, 400,
               errorResponse("invalid_request", "include debe ser basic, metadata o technical."));
      return true;
    }

    json models = context.modelRegistry.list(includeMode);
    sendJson(clientSocket, 200,
             successResponse("Modelos listados correctamente.",
                             json{{"total", models.size()},
                                  {"include", includeMode},
                                  {"cached", true},
                                  {"refresh_seconds", context.options.modelsRefreshSeconds},
                                  {"models", models}}));
    return true;
  }

  auto imageModelName = routeModelImageName(target.path);
  if (!imageModelName) {
    return false;
  }

  if (request.method != "GET") {
    sendMethodNotAllowed(clientSocket);
    return true;
  }

  if (!isSafeFileName(*imageModelName)) {
    sendJson(clientSocket, 400,
             errorResponse("invalid_request", "Nombre de modelo inválido."));
    return true;
  }

  try {
    auto modelInfo = findModelByName(context.options, *imageModelName, &context.modelRegistry);
    if (!modelInfo || !modelInfo->hasConfig) {
      sendJson(clientSocket, 404,
               errorResponse("not_found", "Modelo o configuración no encontrada."));
      return true;
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
        return true;
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

  return true;
}

} // namespace piper_server
