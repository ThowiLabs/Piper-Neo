#include "../http.hpp"

#include <array>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>

#include "../responses.hpp"

namespace piper_server {
namespace {

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

} // namespace

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
