#include "builtin_renderers.hpp"

#include <sstream>
#include <vector>

#include "spanish_numbers.hpp"
#include "string_utils.hpp"

namespace piper::textnorm {

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

std::string percentageToSpanish(const std::string &integerPart,
                                const std::string &fractionPart) {
  if (!fractionPart.empty()) {
    return decimalDigitsToSpeechText(integerPart, fractionPart) + " por ciento";
  }
  return integerPart + " por ciento";
}

} // namespace piper::textnorm
