#include "auth.hpp"

#include "http.hpp"
#include "utils.hpp"

namespace piper_server {

std::optional<std::string> extractBearerToken(const std::string &authorization) {
  const std::string prefix = "Bearer ";
  if (authorization.rfind(prefix, 0) != 0) {
    return std::nullopt;
  }

  auto token = trimCopy(authorization.substr(prefix.size()));
  if (token.empty()) {
    return std::nullopt;
  }

  return token;
}

bool requestIsAuthorized(const HttpRequest &request, const ServerOptions &options) {
  if (options.apiToken.empty()) {
    return true;
  }

  if (auto authorization = getHeader(request, "authorization")) {
    if (auto bearer = extractBearerToken(*authorization)) {
      if (*bearer == options.apiToken) {
        return true;
      }
    }
  }

  if (auto token = getHeader(request, "x-api-token")) {
    if (trimCopy(*token) == options.apiToken) {
      return true;
    }
  }

  return false;
}

} // namespace piper_server
