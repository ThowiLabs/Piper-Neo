#include "builtin_normalizer.hpp"

#include "builtin_matchers.hpp"
#include "builtin_renderers.hpp"
#include "protected_segments.hpp"
#include "spanish_numbers.hpp"
#include "string_utils.hpp"

#include <utility>

namespace piper::textnorm {

namespace {

bool matchUrl(const std::string &text, std::size_t index, std::smatch &match) {
  return prefixRegexMatch(text, index, builtinPatterns().url, match);
}

bool matchBoundedToken(const std::string &text, std::size_t index,
                       const std::regex &regex, std::smatch &match) {
  if (!prefixRegexMatch(text, index, regex, match)) {
    return false;
  }
  const auto tokenSize = match.str(0).size();
  return isSafeLeftBoundary(text, index) && isSafeRightBoundary(text, index + tokenSize);
}

void appendSegment(BuiltinNormalizationResult &result,
                   const std::string &speechText) {
  ProtectedTextBuffer buffer{result.text, result.protectedSegments};
  appendProtectedSegment(buffer, speechText);
  result.text = std::move(buffer.text);
  result.protectedSegments = std::move(buffer.segments);
}

bool consumeUrl(const TextNormalizationBuiltinConfig &builtin,
                const std::string &text, std::size_t &index,
                BuiltinNormalizationResult &result, std::smatch &match) {
  if (!builtin.urls || !matchUrl(text, index, match)) {
    return false;
  }

  const auto rawToken = match.str(1);
  auto token = rawToken;
  const auto trailing = stripTerminalPunctuation(token);
  appendSegment(result, urlToSpanish(token));
  result.text += trailing;
  index += rawToken.size();
  return true;
}

bool consumeEmail(const TextNormalizationBuiltinConfig &builtin,
                  const std::string &text, std::size_t &index,
                  BuiltinNormalizationResult &result, std::smatch &match) {
  if (!builtin.emails ||
      !prefixRegexMatch(text, index, builtinPatterns().email, match) ||
      !isSafeLeftBoundary(text, index)) {
    return false;
  }

  const auto token = match.str(1);
  appendSegment(result, emailToSpanish(token));
  index += token.size();
  return true;
}

bool consumeVersion(const TextNormalizationBuiltinConfig &builtin,
                    const std::string &text, std::size_t &index,
                    BuiltinNormalizationResult &result, std::smatch &match) {
  if (!builtin.versions ||
      !matchBoundedToken(text, index, builtinPatterns().version, match)) {
    return false;
  }

  const auto token = match.str(1);
  appendSegment(result, versionToSpanish(token));
  index += token.size();
  return true;
}

bool consumeCurrency(const TextNormalizationBuiltinConfig &builtin,
                     const std::string &text, std::size_t &index,
                     BuiltinNormalizationResult &result, std::smatch &match) {
  if (!builtin.currency ||
      !prefixRegexMatch(text, index, builtinPatterns().currencyPrefix, match) ||
      !isSafeLeftBoundary(text, index)) {
    return false;
  }

  const auto token = match.str(0);
  appendSegment(result, currencyToSpanish(match.str(1), match.str(4),
                                          match.str(2), match.str(3)));
  index += token.size();
  return true;
}

bool consumePercentage(const TextNormalizationBuiltinConfig &builtin,
                       const std::string &text, std::size_t &index,
                       BuiltinNormalizationResult &result, std::smatch &match) {
  if (!builtin.percentages ||
      !prefixRegexMatch(text, index, builtinPatterns().percent, match) ||
      !isSafeLeftBoundary(text, index)) {
    return false;
  }

  const auto token = match.str(0);
  if (!match.str(1).empty()) {
    appendSegment(result, percentageToSpanish(match.str(1), match.str(2)));
  } else {
    appendSegment(result, percentageToSpanish(match.str(3), {}));
  }
  index += token.size();
  return true;
}

bool consumeDecimal(const TextNormalizationBuiltinConfig &builtin,
                    const std::string &text, std::size_t &index,
                    BuiltinNormalizationResult &result, std::smatch &match) {
  if (!builtin.decimals ||
      !matchBoundedToken(text, index, builtinPatterns().decimal, match)) {
    return false;
  }

  const auto token = match.str(0);
  appendSegment(result, decimalToSpanish(match.str(1), match.str(2)));
  index += token.size();
  return true;
}

} // namespace

BuiltinNormalizationResult normalizeBuiltins(
    const std::string &text, const TextNormalizationBuiltinConfig &builtin) {
  BuiltinNormalizationResult result;
  result.text.reserve(text.size());

  for (std::size_t i = 0; i < text.size();) {
    std::smatch match;
    if (consumeUrl(builtin, text, i, result, match) ||
        consumeEmail(builtin, text, i, result, match) ||
        consumeVersion(builtin, text, i, result, match) ||
        consumeCurrency(builtin, text, i, result, match) ||
        consumePercentage(builtin, text, i, result, match) ||
        consumeDecimal(builtin, text, i, result, match)) {
      continue;
    }

    result.text.push_back(text[i]);
    ++i;
  }

  return result;
}

} // namespace piper::textnorm
