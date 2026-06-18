#ifndef PIPER_SERVER_HTTP_H_
#define PIPER_SERVER_HTTP_H_

#include <map>
#include <optional>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif

#include "types.hpp"

namespace piper_server {

#ifdef _WIN32
using SocketHandle = SOCKET;
constexpr SocketHandle INVALID_SOCKET_HANDLE = INVALID_SOCKET;
#else
using SocketHandle = int;
constexpr SocketHandle INVALID_SOCKET_HANDLE = -1;
#endif

void closeSocket(SocketHandle socketHandle);
std::optional<std::string> getHeader(const HttpRequest &request,
                                     const std::string &headerName);
bool clientDisconnected(SocketHandle socketHandle);
std::optional<HttpRequest> readHttpRequest(SocketHandle socketHandle,
                                           std::size_t maxBodyBytes);
void sendRaw(SocketHandle socketHandle, const std::string &data);
void sendResponse(SocketHandle socketHandle, int statusCode,
                  const std::string &contentType, const std::string &body,
                  const std::map<std::string, std::string> &extraHeaders = {});
void sendJson(SocketHandle socketHandle, int statusCode, const json &body);
ParsedTarget parseTarget(const std::string &rawPath);
std::optional<std::string> queryValue(const ParsedTarget &target,
                                      const std::string &name);
std::optional<std::string> routeFileName(const std::string &path);
std::optional<std::string> routeModelImageName(const std::string &path);
void sendFile(SocketHandle socketHandle, const std::filesystem::path &filePath);

} // namespace piper_server

#endif // PIPER_SERVER_HTTP_H_
