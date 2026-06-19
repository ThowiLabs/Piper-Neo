#include "core/text/utf8_utils.hpp"

namespace piper::text {

bool isUtf8ContinuationByte(char value) {
  return (static_cast<unsigned char>(value) & 0xC0) == 0x80;
}

std::size_t clampToUtf8Boundary(const std::string &text, std::size_t index) {
  if (index >= text.size()) {
    return text.size();
  }

  while ((index > 0) && isUtf8ContinuationByte(text[index])) {
    --index;
  }

  return index;
}

bool isAsciiWhitespace(char value) {
  return (value == ' ') || (value == '\n') || (value == '\r') ||
         (value == '\t') || (value == '\v') || (value == '\f');
}

bool startsWithBytes(const std::string &text, std::size_t index,
                     std::initializer_list<unsigned char> bytes) {
  if ((index + bytes.size()) > text.size()) {
    return false;
  }

  std::size_t offset = 0;
  for (auto expected : bytes) {
    if (static_cast<unsigned char>(text[index + offset]) != expected) {
      return false;
    }
    ++offset;
  }

  return true;
}

void trimLeadingAsciiWhitespace(const std::string &text, std::size_t &offset) {
  while ((offset < text.size()) && isAsciiWhitespace(text[offset])) {
    ++offset;
  }
}

} // namespace piper::text
