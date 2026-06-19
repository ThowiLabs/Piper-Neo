#include "server/media/base64.hpp"

#include <array>
#include <cctype>
#include <stdexcept>
#include <string>

namespace piper_server {
namespace {

const std::string BASE64_CHARS =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

} // namespace

std::string decodeBase64(const std::string &encoded) {
  std::array<int, 256> table{};
  table.fill(-1);
  for (int i = 0; i < static_cast<int>(BASE64_CHARS.size()); ++i) {
    table[static_cast<unsigned char>(BASE64_CHARS[i])] = i;
  }

  std::string decoded;
  int val = 0;
  int valb = -8;
  for (unsigned char c : encoded) {
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
      decoded.push_back(static_cast<char>((val >> valb) & 0xFF));
      valb -= 8;
    }
  }
  return decoded;
}

} // namespace piper_server
