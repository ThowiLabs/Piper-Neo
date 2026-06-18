#include "request_handler.hpp"

#include <stdexcept>
#include <string>

#include "auth.hpp"
#include "http.hpp"
#include "responses.hpp"
#include "routes/file_routes.hpp"
#include "routes/health_routes.hpp"
#include "routes/model_routes.hpp"
#include "routes/route_context.hpp"
#include "routes/tts_routes.hpp"

namespace piper_server {

namespace {

bool dispatchRoute(SocketHandle clientSocket, const HttpRequest &request,
                   const ParsedTarget &target, const RouteContext &context) {
  return handleHealthRoute(clientSocket, request, target, context) ||
         handleModelRoute(clientSocket, request, target, context) ||
         handleFileRoute(clientSocket, request, target, context) ||
         handleTtsRoute(clientSocket, request, target, context);
}

void sendUnhandledError(SocketHandle clientSocket, const std::runtime_error &error) {
  const std::string message = error.what();
  if (message == "payload_too_large") {
    sendJson(clientSocket, 413,
             errorResponse("payload_too_large",
                           "El payload excede el límite permitido."));
    return;
  }

  sendJson(clientSocket, 500, errorResponse("server_error", message));
}

} // namespace

void handleClient(SocketHandle clientSocket, const ServerOptions &options, ModelCache &modelCache,
                  ModelRegistry &modelRegistry, FairTtsScheduler &scheduler,
                  ServerMetrics &metrics) {
  (void)modelCache;

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

    RouteContext context{options, modelRegistry, scheduler, metrics};
    if (!dispatchRoute(clientSocket, request, target, context)) {
      sendJson(clientSocket, 404,
               errorResponse("not_found", "Endpoint no encontrado."));
    }
  } catch (const std::runtime_error &e) {
    sendUnhandledError(clientSocket, e);
  } catch (const std::exception &e) {
    sendJson(clientSocket, 500, errorResponse("server_error", e.what()));
  }

  closeSocket(clientSocket);
}

} // namespace piper_server
