#include "../http.hpp"

#include <cstdlib>
#include <optional>
#include <string>

#include "../utils.hpp"

namespace piper_server {
namespace {

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

} // namespace

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

} // namespace piper_server
