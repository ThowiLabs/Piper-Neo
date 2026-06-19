#ifndef PIPER_SERVER_MODEL_METADATA_H_
#define PIPER_SERVER_MODEL_METADATA_H_

#include <string>

#include "../server.hpp"
#include "http_types.hpp"

namespace piper_server {

bool modelJsonHasImage(const json &root);
json modelInfoToJson(const ModelInfo &modelInfo, const std::string &includeMode);

} // namespace piper_server

#endif // PIPER_SERVER_MODEL_METADATA_H_
