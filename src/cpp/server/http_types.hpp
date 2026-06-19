#ifndef PIPER_SERVER_HTTP_TYPES_H_
#define PIPER_SERVER_HTTP_TYPES_H_

#include <map>
#include <string>

#include "../json.hpp"

namespace piper_server {

using json = nlohmann::json;

struct HttpRequest {
  std::string method;
  std::string path;
  std::string body;
  std::map<std::string, std::string> headers;
};

struct ParsedTarget {
  std::string path;
  std::map<std::string, std::string> query;
};

} // namespace piper_server

#endif // PIPER_SERVER_HTTP_TYPES_H_
