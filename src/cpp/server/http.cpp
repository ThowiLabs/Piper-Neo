#include "http.hpp"

#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

#include "utils.hpp"

namespace piper_server {

bool recvAppend(SocketHandle socketHandle, std::string &buffer);

std::optional<std::string> getHeader(const HttpRequest &request,
                                     const std::string &headerName) {
  auto it = request.headers.find(lowerCopy(headerName));
  if (it == request.headers.end()) {
    return std::nullopt;
  }

  return it->second;
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

} // namespace piper_server
