#include "server/media/data_image.hpp"

#include <stdexcept>
#include <string>
#include <utility>

#include "server/media/base64.hpp"
#include "server/utils.hpp"

namespace piper_server {

std::pair<std::string, std::string> parseDataImage(const std::string &dataUri) {
  const std::string prefix = "data:image/";
  if (dataUri.rfind(prefix, 0) != 0) {
    throw std::runtime_error("invalid_image");
  }
  const auto comma = dataUri.find(',');
  if (comma == std::string::npos) {
    throw std::runtime_error("invalid_image");
  }
  const auto meta = dataUri.substr(0, comma);
  const auto lowerMeta = lowerCopy(meta);
  if (lowerMeta.find(";base64") == std::string::npos) {
    throw std::runtime_error("invalid_image");
  }

  std::string contentType;
  if (lowerMeta.rfind("data:image/jpeg", 0) == 0 || lowerMeta.rfind("data:image/jpg", 0) == 0) {
    contentType = "image/jpeg";
  } else if (lowerMeta.rfind("data:image/png", 0) == 0) {
    contentType = "image/png";
  } else if (lowerMeta.rfind("data:image/webp", 0) == 0) {
    contentType = "image/webp";
  } else {
    throw std::runtime_error("invalid_image");
  }

  return {contentType, decodeBase64(dataUri.substr(comma + 1))};
}

} // namespace piper_server
