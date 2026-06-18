#ifndef PIPER_SERVER_AUTH_H_
#define PIPER_SERVER_AUTH_H_

#include <optional>
#include <string>

#include "../server.hpp"
#include "types.hpp"

namespace piper_server {

std::optional<std::string> extractBearerToken(const std::string &authorization);
bool requestIsAuthorized(const HttpRequest &request, const ServerOptions &options);

} // namespace piper_server

#endif // PIPER_SERVER_AUTH_H_
