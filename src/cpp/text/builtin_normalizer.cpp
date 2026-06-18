#include "builtin_normalizer.hpp"

#include <regex>
#include <sstream>

#include "spanish_numbers.hpp"
#include "string_utils.hpp"

namespace piper::textnorm {

namespace {

std::string makeProtectedMarker(std::size_t index) {
  return std::string("\x1F") + std::to_string(index) + "\x1F";
}

void appendProtectedSegment(BuiltinNormalizationResult &result,
                            const std::string &speechText) {
  const auto index = result.protectedSegments.size();
  result.text += makeProtectedMarker(index);
  result.protectedSegments.push_back(speechText);
}

std::string versionToSpanish(const std::string &version) {
  const bool hasVPrefix = !version.empty() && (version[0] == 'v' || version[0] == 'V');
  const auto raw = hasVPrefix ? version.substr(1) : version;

  std::vector<std::string> parts;
  std::stringstream ss(raw);
  std::string item;
  while (std::getline(ss, item, '.')) {
    parts.push_back(numericGroupToSpanish(item));
  }

  auto out = joinWords(parts, " punto ");
  if (hasVPrefix) {
    out = "versión " + out;
  }
  return out;
}

std::string emailToSpanish(const std::string &email) {
  std::string out;
  for (char c : email) {
    if (c == '@') {
      out += " arroba ";
    } else if (c == '.') {
      out += " punto ";
    } else if (c == '_') {
      out += " guion bajo ";
    } else if (c == '-') {
      out += " guion ";
    } else if (c == '+') {
      out += " más ";
    } else {
      out.push_back(c);
    }
  }
  return out;
}

std::string urlToSpanish(const std::string &url) {
  std::string value = url;
  const auto lower = lowerAsciiCopy(value);
  if (lower.rfind("https://", 0) == 0) {
    value = value.substr(8);
  } else if (lower.rfind("http://", 0) == 0) {
    value = value.substr(7);
  } else if (lower.rfind("www.", 0) == 0) {
    value = value.substr(4);
  }

  std::string out;
  for (char c : value) {
    if (c == '.') {
      out += " punto ";
    } else if (c == '/') {
      out += " diagonal ";
    } else if (c == '-') {
      out += " guion ";
    } else if (c == '_') {
      out += " guion bajo ";
    } else if (c == '?') {
      out += " signo de pregunta ";
    } else if (c == '&') {
      out += " y ";
    } else if (c == '=') {
      out += " igual ";
    } else if (c == ':') {
      out += " dos puntos ";
    } else {
      out.push_back(c);
    }
  }
  return out;
}

std::string currencyToSpanish(const std::string &currencyRaw,
                              const std::string &suffixRaw,
                              const std::string &integerPart,
                              const std::string &fractionPart) {
  const auto currency = lowerAsciiCopy(trimCopy(currencyRaw));
  const auto suffix = lowerAsciiCopy(trimCopy(suffixRaw));
  const bool isUsd = (currency == "usd") || (suffix == "usd") ||
                     (suffix == "dolares") || (suffix == "dólares");

  // No se convierten decimales monetarios a centavos. En contenido hablado
  // para redes, noticias y tutoriales, `$99.50 pesos` debe conservarse como
  // `99 punto 50 pesos`, sin inventar `con cincuenta centavos`.
  std::string out = decimalDigitsToSpeechText(integerPart, fractionPart);
  out += isUsd ? " dólares" : " pesos";
  return out;
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

} // namespace

std::string restoreProtectedSegments(const std::string &text,
                                     const std::vector<std::string> &segments) {
  std::string out;
  out.reserve(text.size());

  for (std::size_t i = 0; i < text.size();) {
    if (text[i] == '\x1F') {
      const auto end = text.find('\x1F', i + 1);
      if (end != std::string::npos) {
        const auto indexText = text.substr(i + 1, end - i - 1);
        try {
          const auto index = static_cast<std::size_t>(std::stoul(indexText));
          if (index < segments.size()) {
            out += segments[index];
            i = end + 1;
            continue;
          }
        } catch (...) {
        }
      }
    }

    out.push_back(text[i]);
    ++i;
  }

  return out;
}

BuiltinNormalizationResult normalizeBuiltins(
    const std::string &text, const TextNormalizationBuiltinConfig &builtin) {
  static const std::regex urlRegex(
      R"(((?:https?://|www\.)[A-Za-z0-9._~:/?#\[\]@!$&'()*+,;=%-]+))",
      std::regex::ECMAScript | std::regex::icase);
  static const std::regex emailRegex(
      R"(([A-Za-z0-9._%+\-]+@[A-Za-z0-9.\-]+\.[A-Za-z]{2,}))",
      std::regex::ECMAScript);
  static const std::regex versionRegex(R"((v?\d+(?:\.\d+){2,}))",
                                      std::regex::ECMAScript | std::regex::icase);
  static const std::regex currencyPrefixRegex(
      R"((\$|MXN\s+|USD\s+)(\d+)(?:\.(\d+))?(?:\s*(pesos?|mxn|USD|dolares|dólares))?)",
      std::regex::ECMAScript | std::regex::icase);
  static const std::regex percentRegex(R"((\d+)\.(\d+)%|(\d+)%)",
                                      std::regex::ECMAScript);
  static const std::regex decimalRegex(R"((\d+)\.(\d+))",
                                      std::regex::ECMAScript);

  BuiltinNormalizationResult result;
  result.text.reserve(text.size());

  for (std::size_t i = 0; i < text.size();) {
    std::smatch match;

    if (builtin.urls && prefixRegexMatch(text, i, urlRegex, match)) {
      const auto rawToken = match.str(1);
      auto token = rawToken;
      const auto trailing = stripTerminalPunctuation(token);
      appendProtectedSegment(result, urlToSpanish(token));
      result.text += trailing;
      i += rawToken.size();
      continue;
    }

    if (builtin.emails && prefixRegexMatch(text, i, emailRegex, match) &&
        isSafeLeftBoundary(text, i)) {
      const auto token = match.str(1);
      appendProtectedSegment(result, emailToSpanish(token));
      i += token.size();
      continue;
    }

    if (builtin.versions && prefixRegexMatch(text, i, versionRegex, match) &&
        isSafeLeftBoundary(text, i) && isSafeRightBoundary(text, i + match.str(1).size())) {
      const auto token = match.str(1);
      appendProtectedSegment(result, versionToSpanish(token));
      i += token.size();
      continue;
    }

    if (builtin.currency && prefixRegexMatch(text, i, currencyPrefixRegex, match) &&
        isSafeLeftBoundary(text, i)) {
      const auto token = match.str(0);
      appendProtectedSegment(result,
                             currencyToSpanish(match.str(1), match.str(4),
                                               match.str(2), match.str(3)));
      i += token.size();
      continue;
    }

    if (builtin.percentages && prefixRegexMatch(text, i, percentRegex, match) &&
        isSafeLeftBoundary(text, i)) {
      const auto token = match.str(0);
      if (!match.str(1).empty()) {
        appendProtectedSegment(result,
                               decimalDigitsToSpeechText(match.str(1), match.str(2)) +
                                   " por ciento");
      } else {
        appendProtectedSegment(result, match.str(3) + " por ciento");
      }
      i += token.size();
      continue;
    }

    if (builtin.decimals && prefixRegexMatch(text, i, decimalRegex, match) &&
        isSafeLeftBoundary(text, i) && isSafeRightBoundary(text, i + match.str(0).size())) {
      const auto token = match.str(0);
      appendProtectedSegment(result, decimalToSpanish(match.str(1), match.str(2)));
      i += token.size();
      continue;
    }

    result.text.push_back(text[i]);
    ++i;
  }

  return result;
}

} // namespace piper::textnorm
