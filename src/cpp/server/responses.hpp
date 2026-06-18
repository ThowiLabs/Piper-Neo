#ifndef PIPER_SERVER_RESPONSES_H_
#define PIPER_SERVER_RESPONSES_H_

#include <string>

#include "types.hpp"

namespace piper_server {

json successResponse(const std::string &message, const json &data = json::object());
json errorResponse(const std::string &error, const std::string &message);
int errorStatusForException(const std::string &error);
json modelErrorResponse(const std::string &error);

} // namespace piper_server

#endif // PIPER_SERVER_RESPONSES_H_
