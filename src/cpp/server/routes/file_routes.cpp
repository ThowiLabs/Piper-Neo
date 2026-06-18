#include "server/routes/file_routes.hpp"

#include "server/responses.hpp"
#include "server/utils.hpp"

namespace piper_server {

bool handleFileRoute(SocketHandle clientSocket, const HttpRequest &request,
                     const ParsedTarget &target, const RouteContext &context) {
  auto fileName = routeFileName(target.path);
  if (!fileName) {
    return false;
  }

  if (request.method != "GET") {
    sendJson(clientSocket, 405,
             errorResponse("method_not_allowed", "Método no permitido."));
    return true;
  }

  if (!isSafeFileName(*fileName)) {
    sendJson(clientSocket, 400,
             errorResponse("invalid_request", "Nombre de archivo inválido."));
    return true;
  }

  sendFile(clientSocket, context.options.outputDir / *fileName);
  return true;
}

} // namespace piper_server
