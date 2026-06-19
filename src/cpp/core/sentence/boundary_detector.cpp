#include "core/sentence/boundary_detector.hpp"

#include <algorithm>
#include <initializer_list>
#include <string>
#include <vector>

namespace piper::core::sentence {
namespace {

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

bool isUtf8Ellipsis(const std::string &text, std::size_t index) {
  // UTF-8: … = E2 80 A6
  return startsWithBytes(text, index, {0xE2, 0x80, 0xA6});
}

bool isClosingQuoteOrBracket(char value) {
  return (value == '"') || (value == '\'') || (value == ')') ||
         (value == ']') || (value == '}') || (value == '>');
}

std::size_t skipClosingMarksForward(const std::string &text,
                                    std::size_t index,
                                    std::size_t limit) {
  while (index < limit) {
    if (isClosingQuoteOrBracket(text[index])) {
      ++index;
      continue;
    }

    // UTF-8: ” = E2 80 9D, ’ = E2 80 99, » = C2 BB
    if (startsWithBytes(text, index, {0xE2, 0x80, 0x9D}) ||
        startsWithBytes(text, index, {0xE2, 0x80, 0x99})) {
      index += 3;
      continue;
    }

    if (startsWithBytes(text, index, {0xC2, 0xBB})) {
      index += 2;
      continue;
    }

    break;
  }

  return index;
}

bool hasBoundaryAfter(const std::string &text, std::size_t splitAt) {
  splitAt = skipClosingMarksForward(text, splitAt, text.size());
  return (splitAt >= text.size()) || isAsciiWhitespace(text[splitAt]);
}

bool isAsciiDigitAt(const std::string &text, std::size_t index) {
  return (index < text.size()) && (text[index] >= '0') && (text[index] <= '9');
}

bool isAsciiLetterAt(const std::string &text, std::size_t index) {
  if (index >= text.size()) {
    return false;
  }
  const unsigned char value = static_cast<unsigned char>(text[index]);
  return ((value >= 'A') && (value <= 'Z')) || ((value >= 'a') && (value <= 'z'));
}

std::string lowerAsciiCopy(std::string value) {
  for (auto &ch : value) {
    if ((ch >= 'A') && (ch <= 'Z')) {
      ch = static_cast<char>(ch - 'A' + 'a');
    }
  }
  return value;
}

std::string previousAsciiToken(const std::string &text, std::size_t periodIndex) {
  if (periodIndex == 0) {
    return {};
  }

  std::size_t end = periodIndex;
  while ((end > 0) && isAsciiWhitespace(text[end - 1])) {
    --end;
  }

  std::size_t begin = end;
  while ((begin > 0) && isAsciiLetterAt(text, begin - 1)) {
    --begin;
  }

  return text.substr(begin, end - begin);
}

bool isLikelyAbbreviationPeriod(const std::string &text, std::size_t periodIndex) {
  const auto token = lowerAsciiCopy(previousAsciiToken(text, periodIndex));
  if (token.empty()) {
    return false;
  }

  static const std::vector<std::string> abbreviations = {
      "sr", "sra", "srta", "dr", "dra", "lic", "ing", "arq", "mtro",
      "mtra", "prof", "profa", "etc", "ej", "p", "pag", "tel", "av",
      "col", "num", "no", "vs", "ud", "uds"};

  return std::find(abbreviations.begin(), abbreviations.end(), token) != abbreviations.end();
}

bool isSentencePeriodBoundary(const std::string &text, std::size_t index) {
  if (text[index] != '.') {
    return false;
  }

  // Decimal numbers and version-like values should not create a sentence pause.
  if ((index > 0) && isAsciiDigitAt(text, index - 1) && isAsciiDigitAt(text, index + 1)) {
    return false;
  }

  if (isLikelyAbbreviationPeriod(text, index)) {
    return false;
  }

  const std::size_t splitAt = skipClosingMarksForward(text, index + 1, text.size());
  return (splitAt >= text.size()) || isAsciiWhitespace(text[splitAt]);
}

bool isLineBreakBoundary(const std::string &text, std::size_t index,
                         std::size_t &splitAt) {
  const char value = text[index];
  if ((value != '\n') && (value != '\r')) {
    return false;
  }

  splitAt = index + 1;
  if ((value == '\r') && (splitAt < text.size()) && (text[splitAt] == '\n')) {
    ++splitAt;
  }

  // Consecutive line breaks represent the same intentional pause. Collapse them
  // into one boundary so empty lines do not synthesize empty segments.
  while (splitAt < text.size()) {
    if (text[splitAt] == '\n') {
      ++splitAt;
      continue;
    }

    if (text[splitAt] == '\r') {
      ++splitAt;
      if ((splitAt < text.size()) && (text[splitAt] == '\n')) {
        ++splitAt;
      }
      continue;
    }

    break;
  }

  return true;
}

} // namespace

bool isUtf8ContinuationByte(char value) {
  return (static_cast<unsigned char>(value) & 0xC0) == 0x80;
}

bool isAsciiWhitespace(char value) {
  return (value == ' ') || (value == '\n') || (value == '\r') ||
         (value == '\t') || (value == '\v') || (value == '\f');
}

bool isExplicitSentenceBoundary(const std::string &text, std::size_t index,
                                std::size_t &splitAt) {
  if (index >= text.size()) {
    return false;
  }

  const char value = text[index];
  if (value == '.') {
    if (!isSentencePeriodBoundary(text, index)) {
      return false;
    }
    splitAt = skipClosingMarksForward(text, index + 1, text.size());
    return true;
  }

  if ((value == '!') || (value == '?')) {
    splitAt = skipClosingMarksForward(text, index + 1, text.size());
    return hasBoundaryAfter(text, index + 1);
  }

  if (isUtf8Ellipsis(text, index)) {
    splitAt = skipClosingMarksForward(text, index + 3, text.size());
    return hasBoundaryAfter(text, index + 3);
  }

  return isLineBreakBoundary(text, index, splitAt);
}

} // namespace piper::core::sentence
