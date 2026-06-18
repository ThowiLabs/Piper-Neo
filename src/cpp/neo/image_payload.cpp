#include "neo/image_payload.hpp"

#include <array>
#include <cctype>
#include <stdexcept>

#include "neo/file_utils.hpp"

namespace piper_neo::detail {

std::string contentTypeForImagePath(const std::filesystem::path &path) {
  const auto ext = lowerCopy(path.extension().string());
  if (ext == ".jpg" || ext == ".jpeg") {
    return "image/jpeg";
  }
  if (ext == ".png") {
    return "image/png";
  }
  if (ext == ".webp") {
    return "image/webp";
  }
  return "application/octet-stream";
}

std::pair<std::string, std::vector<char>> decodeDataImage(const std::string &dataUri) {
  static const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  const std::string prefix = "data:image/";
  if (dataUri.rfind(prefix, 0) != 0) {
    throw std::runtime_error("invalid_image");
  }

  const auto comma = dataUri.find(',');
  if (comma == std::string::npos) {
    throw std::runtime_error("invalid_image");
  }

  auto meta = lowerCopy(dataUri.substr(0, comma));
  if (meta.find(";base64") == std::string::npos) {
    throw std::runtime_error("invalid_image");
  }

  std::string contentType;
  if (meta.rfind("data:image/jpeg", 0) == 0 || meta.rfind("data:image/jpg", 0) == 0) {
    contentType = "image/jpeg";
  } else if (meta.rfind("data:image/png", 0) == 0) {
    contentType = "image/png";
  } else if (meta.rfind("data:image/webp", 0) == 0) {
    contentType = "image/webp";
  } else {
    throw std::runtime_error("invalid_image");
  }

  std::array<int, 256> table{};
  table.fill(-1);
  for (int i = 0; i < static_cast<int>(chars.size()); ++i) {
    table[static_cast<unsigned char>(chars[i])] = i;
  }

  std::vector<char> decoded;
  int val = 0;
  int valb = -8;
  for (std::size_t i = comma + 1; i < dataUri.size(); ++i) {
    unsigned char c = static_cast<unsigned char>(dataUri[i]);
    if (std::isspace(c)) {
      continue;
    }
    if (c == '=') {
      break;
    }
    if (table[c] == -1) {
      throw std::runtime_error("invalid_image");
    }
    val = (val << 6) + table[c];
    valb += 6;
    if (valb >= 0) {
      decoded.push_back(static_cast<char>((val >> valb) & 0xff));
      valb -= 8;
    }
  }

  return {contentType, decoded};
}

} // namespace piper_neo::detail
