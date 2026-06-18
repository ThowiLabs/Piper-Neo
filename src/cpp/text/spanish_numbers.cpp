#include "spanish_numbers.hpp"

#include <sstream>

namespace piper::textnorm {

std::string joinWords(const std::vector<std::string> &words,
                      const std::string &sep) {
  std::string out;
  for (std::size_t i = 0; i < words.size(); ++i) {
    if (i > 0) {
      out += sep;
    }
    out += words[i];
  }
  return out;
}

std::string digitToSpanish(char digit) {
  switch (digit) {
  case '0': return "cero";
  case '1': return "uno";
  case '2': return "dos";
  case '3': return "tres";
  case '4': return "cuatro";
  case '5': return "cinco";
  case '6': return "seis";
  case '7': return "siete";
  case '8': return "ocho";
  case '9': return "nueve";
  default: return std::string(1, digit);
  }
}

std::string digitsToSpanish(const std::string &digits) {
  std::vector<std::string> words;
  words.reserve(digits.size());
  for (char digit : digits) {
    words.push_back(digitToSpanish(digit));
  }
  return joinWords(words);
}

namespace {

std::string twoDigitsToSpanish(int value) {
  static const char *units[] = {"cero", "uno", "dos", "tres", "cuatro",
                                "cinco", "seis", "siete", "ocho", "nueve"};
  static const char *tens[] = {"", "", "veinte", "treinta", "cuarenta",
                               "cincuenta", "sesenta", "setenta", "ochenta", "noventa"};

  if (value < 10) {
    return units[value];
  }
  if (value == 10) return "diez";
  if (value == 11) return "once";
  if (value == 12) return "doce";
  if (value == 13) return "trece";
  if (value == 14) return "catorce";
  if (value == 15) return "quince";
  if (value < 20) return std::string("dieci") + units[value - 10];
  if (value == 20) return "veinte";
  if (value < 30) return std::string("veinti") + units[value - 20];

  const int ten = value / 10;
  const int unit = value % 10;
  if (unit == 0) {
    return tens[ten];
  }
  return std::string(tens[ten]) + " y " + units[unit];
}

std::string threeDigitsToSpanish(int value) {
  if (value < 100) {
    return twoDigitsToSpanish(value);
  }
  if (value == 100) {
    return "cien";
  }

  static const char *hundreds[] = {"", "ciento", "doscientos", "trescientos",
                                   "cuatrocientos", "quinientos", "seiscientos",
                                   "setecientos", "ochocientos", "novecientos"};
  const int hundred = value / 100;
  const int rest = value % 100;
  if (rest == 0) {
    return hundreds[hundred];
  }
  return std::string(hundreds[hundred]) + " " + twoDigitsToSpanish(rest);
}

long long parseIntegerSafe(const std::string &digits) {
  try {
    return std::stoll(digits);
  } catch (...) {
    return 0;
  }
}

} // namespace

std::string integerToSpanish(long long value) {
  if (value == 0) {
    return "cero";
  }
  if (value < 0) {
    return "menos " + integerToSpanish(-value);
  }
  if (value < 1000) {
    return threeDigitsToSpanish(static_cast<int>(value));
  }
  if (value < 1000000) {
    const auto thousands = value / 1000;
    const auto rest = value % 1000;
    std::string out = (thousands == 1) ? "mil" : (integerToSpanish(thousands) + " mil");
    if (rest > 0) {
      out += " " + threeDigitsToSpanish(static_cast<int>(rest));
    }
    return out;
  }
  if (value < 1000000000000LL) {
    const auto millions = value / 1000000;
    const auto rest = value % 1000000;
    std::string out = (millions == 1) ? "un millón" : (integerToSpanish(millions) + " millones");
    if (rest > 0) {
      out += " " + integerToSpanish(rest);
    }
    return out;
  }

  return digitsToSpanish(std::to_string(value));
}

std::string numericGroupToSpanish(const std::string &digits) {
  if (digits.empty()) {
    return {};
  }
  if (digits.size() > 1 && digits[0] == '0') {
    return digitsToSpanish(digits);
  }
  if (digits.size() > 12) {
    return digitsToSpanish(digits);
  }
  return integerToSpanish(parseIntegerSafe(digits));
}

std::string decimalToSpanish(const std::string &integerPart,
                             const std::string &fractionPart) {
  std::string out = numericGroupToSpanish(integerPart) + " punto ";
  if (fractionPart.size() > 1 && fractionPart[0] == '0') {
    out += digitsToSpanish(fractionPart);
  } else {
    out += numericGroupToSpanish(fractionPart);
  }
  return out;
}

std::string decimalDigitsToSpeechText(const std::string &integerPart,
                                      const std::string &fractionPart) {
  if (fractionPart.empty()) {
    return integerPart;
  }
  return integerPart + " punto " + fractionPart;
}

} // namespace piper::textnorm
