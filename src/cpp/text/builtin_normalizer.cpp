#include "builtin_normalizer.hpp"

#include "builtin_matchers.hpp"
#include "builtin_renderers.hpp"
#include "protected_segments.hpp"
#include "string_utils.hpp"

#include <utility>

namespace piper::textnorm {

namespace {

bool matchUrl(const std::string &text, std::size_t index, std::smatch &match) {
  return prefixRegexMatch(text, index, builtinPatterns().url, match);
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
  appendSegment(result, urlToSpeechText(token));
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
  appendSegment(result, emailToSpeechText(token));
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
        consumeEmail(builtin, text, i, result, match)) {
      continue;
    }

    result.text.push_back(text[i]);
    ++i;
  }

  return result;
}

} // namespace piper::textnorm
