#include "http.hpp"

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#else
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "responses.hpp"
#include "utils.hpp"

namespace piper_server {

void closeSocket(SocketHandle socketHandle) {
#ifdef _WIN32
  closesocket(socketHandle);
#else
  close(socketHandle);
#endif
}

std::optional<std::string> getHeader(const HttpRequest &request,
                                     const std::string &headerName) {
  auto it = request.headers.find(lowerCopy(headerName));
  if (it == request.headers.end()) {
    return std::nullopt;
  }

  return it->second;
}

bool clientDisconnected(SocketHandle socketHandle) {
  fd_set readSet;
  FD_ZERO(&readSet);
  FD_SET(socketHandle, &readSet);

  timeval timeout{};
  timeout.tv_sec = 0;
  timeout.tv_usec = 0;

#ifdef _WIN32
  int ready = select(0, &readSet, nullptr, nullptr, &timeout);
#else
  int ready = select(socketHandle + 1, &readSet, nullptr, nullptr, &timeout);
#endif

  if (ready <= 0 || !FD_ISSET(socketHandle, &readSet)) {
    return false;
  }

  char probe = 0;
#ifdef _WIN32
  int received = recv(socketHandle, &probe, 1, MSG_PEEK);
  if (received == 0) {
    return true;
  }
  if (received < 0) {
    int err = WSAGetLastError();
    return (err != WSAEWOULDBLOCK) && (err != WSAEINTR);
  }
#else
  ssize_t received = recv(socketHandle, &probe, 1, MSG_PEEK);
  if (received == 0) {
    return true;
  }
  if (received < 0) {
    return (errno != EAGAIN) && (errno != EWOULDBLOCK) && (errno != EINTR);
  }
#endif

  // The peer may have sent extra bytes on a keep-alive request. This server
  // closes every response, so extra data is ignored but does not mean closed.
  return false;
}

bool recvAppend(SocketHandle socketHandle, std::string &buffer) {
  char chunk[8192];
#ifdef _WIN32
  int received = recv(socketHandle, chunk, static_cast<int>(sizeof(chunk)), 0);
#else
  ssize_t received = recv(socketHandle, chunk, sizeof(chunk), 0);
#endif
  if (received <= 0) {
    return false;
  }

  buffer.append(chunk, static_cast<std::size_t>(received));
  return true;
}

std::optional<HttpRequest> readHttpRequest(SocketHandle socketHandle,
                                           std::size_t maxBodyBytes) {
  std::string buffer;
  std::size_t headerEnd = std::string::npos;

  while ((headerEnd = buffer.find("\r\n\r\n")) == std::string::npos) {
    if (!recvAppend(socketHandle, buffer)) {
      return std::nullopt;
    }

    if (buffer.size() > 64 * 1024) {
      throw std::runtime_error("HTTP headers too large");
    }
  }

  std::string headerBlock = buffer.substr(0, headerEnd);
  std::istringstream headerStream(headerBlock);
  std::string requestLine;
  std::getline(headerStream, requestLine);
  if (!requestLine.empty() && requestLine.back() == '\r') {
    requestLine.pop_back();
  }

  std::istringstream requestLineStream(requestLine);
  HttpRequest request;
  std::string version;
  requestLineStream >> request.method >> request.path >> version;
  if (request.method.empty() || request.path.empty()) {
    throw std::runtime_error("Invalid HTTP request line");
  }

  std::string headerLine;
  while (std::getline(headerStream, headerLine)) {
    if (!headerLine.empty() && headerLine.back() == '\r') {
      headerLine.pop_back();
    }

    auto colon = headerLine.find(':');
    if (colon == std::string::npos) {
      continue;
    }

    auto key = lowerCopy(trimCopy(headerLine.substr(0, colon)));
    auto value = trimCopy(headerLine.substr(colon + 1));
    request.headers[key] = value;
  }

  std::size_t contentLength = 0;
  if (auto header = getHeader(request, "content-length")) {
    contentLength = static_cast<std::size_t>(std::stoull(*header));
  }

  if (contentLength > maxBodyBytes) {
    throw std::runtime_error("payload_too_large");
  }

  const std::size_t bodyStart = headerEnd + 4;
  while ((buffer.size() - bodyStart) < contentLength) {
    if (!recvAppend(socketHandle, buffer)) {
      throw std::runtime_error("Unexpected end of HTTP body");
    }

    if ((buffer.size() - bodyStart) > maxBodyBytes) {
      throw std::runtime_error("payload_too_large");
    }
  }

  request.body = buffer.substr(bodyStart, contentLength);
  return request;
}

void sendRaw(SocketHandle socketHandle, const std::string &data) {
  const char *ptr = data.data();
  std::size_t remaining = data.size();
  while (remaining > 0) {
#ifdef _WIN32
    int sent = send(socketHandle, ptr, static_cast<int>(remaining), 0);
#else
    ssize_t sent = send(socketHandle, ptr, remaining, 0);
#endif
    if (sent <= 0) {
      return;
    }
    ptr += sent;
    remaining -= static_cast<std::size_t>(sent);
  }
}

std::string reasonPhrase(int statusCode) {
  switch (statusCode) {
  case 200:
    return "OK";
  case 201:
    return "Created";
  case 400:
    return "Bad Request";
  case 401:
    return "Unauthorized";
  case 404:
    return "Not Found";
  case 405:
    return "Method Not Allowed";
  case 413:
    return "Payload Too Large";
  case 422:
    return "Unprocessable Content";
  case 429:
    return "Too Many Requests";
  case 500:
    return "Internal Server Error";
  case 507:
    return "Insufficient Storage";
  default:
    return "OK";
  }
}

void sendResponse(SocketHandle socketHandle, int statusCode,
                  const std::string &contentType, const std::string &body,
                  const std::map<std::string, std::string> &extraHeaders) {
  std::ostringstream response;
  response << "HTTP/1.1 " << statusCode << " " << reasonPhrase(statusCode) << "\r\n";
  response << "Content-Type: " << contentType << "\r\n";
  response << "Content-Length: " << body.size() << "\r\n";
  response << "Connection: close\r\n";
  response << "Access-Control-Allow-Origin: *\r\n";
  response << "Access-Control-Allow-Headers: Content-Type, Authorization, X-API-Token\r\n";
  response << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
  for (const auto &[key, value] : extraHeaders) {
    response << key << ": " << value << "\r\n";
  }
  response << "\r\n";
  response << body;
  sendRaw(socketHandle, response.str());
}

void sendJson(SocketHandle socketHandle, int statusCode, const json &body) {
  sendResponse(socketHandle, statusCode, "application/json; charset=utf-8",
               body.dump(2));
}

std::string urlDecode(const std::string &value) {
  std::string decoded;
  decoded.reserve(value.size());
  for (std::size_t i = 0; i < value.size(); ++i) {
    if (value[i] == '%' && (i + 2) < value.size()) {
      const auto hex = value.substr(i + 1, 2);
      char *end = nullptr;
      long code = std::strtol(hex.c_str(), &end, 16);
      if (end != nullptr && *end == '\0') {
        decoded.push_back(static_cast<char>(code));
        i += 2;
        continue;
      }
    }
    if (value[i] == '+') {
      decoded.push_back(' ');
    } else {
      decoded.push_back(value[i]);
    }
  }
  return decoded;
}

ParsedTarget parseTarget(const std::string &rawPath) {
  ParsedTarget target;
  const auto question = rawPath.find('?');
  target.path = question == std::string::npos ? rawPath : rawPath.substr(0, question);
  if (question == std::string::npos) {
    return target;
  }

  std::string queryString = rawPath.substr(question + 1);
  std::size_t offset = 0;
  while (offset <= queryString.size()) {
    const auto amp = queryString.find('&', offset);
    const auto part = queryString.substr(offset, amp == std::string::npos ? std::string::npos : amp - offset);
    if (!part.empty()) {
      const auto equals = part.find('=');
      const auto key = urlDecode(part.substr(0, equals));
      const auto value = equals == std::string::npos ? std::string() : urlDecode(part.substr(equals + 1));
      target.query[lowerCopy(key)] = value;
    }
    if (amp == std::string::npos) {
      break;
    }
    offset = amp + 1;
  }
  return target;
}

std::optional<std::string> queryValue(const ParsedTarget &target,
                                      const std::string &name) {
  auto it = target.query.find(lowerCopy(name));
  if (it == target.query.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::optional<std::string> routeFileName(const std::string &path) {
  const std::string prefix = "/api/v1/files/";
  if (path.rfind(prefix, 0) != 0) {
    return std::nullopt;
  }

  return urlDecode(path.substr(prefix.size()));
}

std::optional<std::string> routeModelImageName(const std::string &path) {
  const std::string prefix = "/api/v1/models/";
  const std::string suffix = "/image";
  if (path.rfind(prefix, 0) != 0 || path.size() <= (prefix.size() + suffix.size())) {
    return std::nullopt;
  }

  if (path.substr(path.size() - suffix.size()) != suffix) {
    return std::nullopt;
  }

  return urlDecode(path.substr(prefix.size(), path.size() - prefix.size() - suffix.size()));
}


void sendFile(SocketHandle socketHandle, const std::filesystem::path &filePath) {
  std::ifstream file(filePath, std::ios::binary);
  if (!file.good()) {
    sendJson(socketHandle, 404,
             errorResponse("not_found", "Archivo no encontrado."));
    return;
  }

  const auto fileSize = std::filesystem::file_size(filePath);
  std::ostringstream headers;
  headers << "HTTP/1.1 200 OK\r\n";
  headers << "Content-Type: audio/wav\r\n";
  headers << "Content-Length: " << fileSize << "\r\n";
  headers << "Connection: close\r\n";
  headers << "Access-Control-Allow-Origin: *\r\n";
  headers << "Content-Disposition: attachment; filename=\""
          << filePath.filename().string() << "\"\r\n";
  headers << "\r\n";
  sendRaw(socketHandle, headers.str());

  std::array<char, 64 * 1024> buffer{};
  while (file.good()) {
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    const auto count = file.gcount();
    if (count > 0) {
      sendRaw(socketHandle, std::string(buffer.data(), static_cast<std::size_t>(count)));
    }
  }
}

} // namespace piper_server
