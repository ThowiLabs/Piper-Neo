#include "builtin_matchers.hpp"

#include "string_utils.hpp"

namespace piper::textnorm {

BuiltinPatterns::BuiltinPatterns()
    : url(R"(((?:https?://|www\.)[A-Za-z0-9._~:/?#\[\]@!$&'()*+,;=%-]+))",
          std::regex::ECMAScript | std::regex::icase),
      email(R"(([A-Za-z0-9._%+\-]+@[A-Za-z0-9.\-]+\.[A-Za-z]{2,}))",
            std::regex::ECMAScript),
      version(R"((v?\d+(?:\.\d+){2,}))",
              std::regex::ECMAScript | std::regex::icase),
      currencyPrefix(
          R"((\$|MXN\s+|USD\s+)(\d+)(?:\.(\d+))?(?:\s*(pesos?|mxn|USD|dolares|dólares))?)",
          std::regex::ECMAScript | std::regex::icase),
      percent(R"((\d+)\.(\d+)%|(\d+)%)", std::regex::ECMAScript),
      decimal(R"((\d+)\.(\d+))", std::regex::ECMAScript) {}

const BuiltinPatterns &builtinPatterns() {
  static const BuiltinPatterns patterns;
  return patterns;
}

bool prefixRegexMatch(const std::string &text, std::size_t index,
                      const std::regex &regex, std::smatch &match) {
  const auto begin = text.cbegin() + static_cast<std::ptrdiff_t>(index);
  return std::regex_search(begin, text.cend(), match, regex,
                           std::regex_constants::match_continuous);
}

bool isSafeLeftBoundary(const std::string &text, std::size_t index) {
  return index == 0 || !isAsciiAlphaNumericOrUnderscore(text[index - 1]);
}

bool isSafeRightBoundary(const std::string &text, std::size_t index) {
  return index >= text.size() || !isAsciiAlphaNumericOrUnderscore(text[index]);
}

} // namespace piper::textnorm
