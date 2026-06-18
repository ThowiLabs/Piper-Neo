#ifndef PIPER_SERVER_ROUTES_TTS_ROUTES_H_
#define PIPER_SERVER_ROUTES_TTS_ROUTES_H_

#include "../http.hpp"
#include "route_context.hpp"

namespace piper_server {

bool handleTtsRoute(SocketHandle clientSocket, const HttpRequest &request,
                    const ParsedTarget &target, const RouteContext &context);

} // namespace piper_server

#endif // PIPER_SERVER_ROUTES_TTS_ROUTES_H_
