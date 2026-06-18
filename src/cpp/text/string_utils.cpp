#include "string_utils.hpp"

#include <cctype>

namespace piper::textnorm {

std::string lowerAsciiCopy(std::string value) {
  for (auto &c : value) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return value;
}

std::string trimCopy(const std::string &value) {
  std::size_t begin = 0;
  while (begin < value.size() &&
         std::isspace(static_cast<unsigned char>(value[begin]))) {
    ++begin;
  }

  std::size_t end = value.size();
  while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
    --end;
  }

  return value.substr(begin, end - begin);
}

bool isAsciiAlphaNumericOrUnderscore(char c) {
  const auto value = static_cast<unsigned char>(c);
  return std::isalnum(value) || value == '_';
}

bool isWholeWordBoundaryBefore(const std::string &text, std::size_t index) {
  return index == 0 || !isAsciiAlphaNumericOrUnderscore(text[index - 1]);
}

bool isWholeWordBoundaryAfter(const std::string &text, std::size_t index) {
  return index >= text.size() || !isAsciiAlphaNumericOrUnderscore(text[index]);
}

bool asciiStartsWithAt(const std::string &text, const std::string &needle,
                       std::size_t index, bool caseSensitive) {
  if (needle.empty() || index + needle.size() > text.size()) {
    return false;
  }

  for (std::size_t i = 0; i < needle.size(); ++i) {
    const auto a = text[index + i];
    const auto b = needle[i];
    if (caseSensitive) {
      if (a != b) {
        return false;
      }
    } else if (std::tolower(static_cast<unsigned char>(a)) !=
               std::tolower(static_cast<unsigned char>(b))) {
      return false;
    }
  }

  return true;
}

bool isTerminalPunctuation(char c) {
  return c == '.' || c == ',' || c == ';' || c == ':' || c == '!' || c == ')';
}

std::string stripTerminalPunctuation(std::string &token) {
  std::string trailing;
  while (!token.empty() && isTerminalPunctuation(token.back())) {
    trailing.insert(trailing.begin(), token.back());
    token.pop_back();
  }
  return trailing;
}

} // namespace piper::textnorm
