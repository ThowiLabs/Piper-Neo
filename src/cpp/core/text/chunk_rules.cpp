#include "core/text/chunk_rules.hpp"

#include "core/text/utf8_utils.hpp"

#include <algorithm>
#include <string>

namespace piper::text {
namespace {

bool isClosingSpanishQuestion(const std::string &text, std::size_t index) {
  return text[index] == '?';
}

bool isClosingSpanishExclamation(const std::string &text, std::size_t index) {
  return text[index] == '!';
}

bool isOpeningSpanishQuestion(const std::string &text, std::size_t index) {
  // UTF-8: ¿ = C2 BF
  return startsWithBytes(text, index, {0xC2, 0xBF});
}

bool isOpeningSpanishExclamation(const std::string &text, std::size_t index) {
  // UTF-8: ¡ = C2 A1
  return startsWithBytes(text, index, {0xC2, 0xA1});
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

bool isStrongSentenceBoundary(const std::string &text, std::size_t index,
                              std::size_t &splitAt) {
  char value = text[index];
  if ((value == '.') || (value == '!') || (value == '?')) {
    splitAt = skipClosingMarksForward(text, index + 1, text.size());
    return hasBoundaryAfter(text, index + 1);
  }

  if (isUtf8Ellipsis(text, index)) {
    splitAt = skipClosingMarksForward(text, index + 3, text.size());
    return hasBoundaryAfter(text, index + 3);
  }

  return false;
}

std::size_t findLastParagraphBoundary(const std::string &text,
                                      std::size_t begin,
                                      std::size_t candidate,
                                      std::size_t minBoundary) {
  std::size_t best = std::string::npos;
  for (std::size_t i = begin + 1; i < candidate; ++i) {
    bool paragraphBreak = ((text[i - 1] == '\n') && (text[i] == '\n')) ||
                          ((text[i - 1] == '\r') && (text[i] == '\n'));
    if (paragraphBreak && ((i + 1) >= minBoundary)) {
      best = i + 1;
    }
  }

  return best;
}

std::size_t findLastStrongSentenceBoundary(const std::string &text,
                                           std::size_t begin,
                                           std::size_t candidate,
                                           std::size_t minBoundary) {
  std::size_t best = std::string::npos;
  for (std::size_t i = begin; i < candidate; ++i) {
    std::size_t splitAt = std::string::npos;
    if (isStrongSentenceBoundary(text, i, splitAt) &&
        (splitAt >= minBoundary) && (splitAt <= candidate)) {
      best = splitAt;
    }
  }

  return best;
}

std::size_t findLastSoftBoundary(const std::string &text, std::size_t begin,
                                 std::size_t candidate,
                                 std::size_t minBoundary) {
  std::size_t best = std::string::npos;
  for (std::size_t i = begin; i < candidate; ++i) {
    char value = text[i];
    if ((value == ';') || (value == ':') || (value == ',') ||
        (value == '\n') || (value == '\r') || (value == '\t') ||
        (value == ' ')) {
      std::size_t splitAt = i + 1;
      if (splitAt >= minBoundary) {
        best = splitAt;
      }
    }
  }

  return best;
}

std::size_t findLastWhitespaceBoundary(const std::string &text,
                                       std::size_t begin,
                                       std::size_t candidate,
                                       std::size_t minBoundary) {
  std::size_t best = std::string::npos;
  for (std::size_t i = begin; i < candidate; ++i) {
    if (isAsciiWhitespace(text[i]) && ((i + 1) >= minBoundary)) {
      best = i + 1;
    }
  }

  return best;
}

std::size_t findForwardWhitespaceBoundary(const std::string &text,
                                          std::size_t candidate,
                                          std::size_t hardLimit) {
  for (std::size_t i = candidate; i < hardLimit; ++i) {
    if (isAsciiWhitespace(text[i])) {
      return i + 1;
    }
  }

  return std::string::npos;
}

std::size_t findUnclosedSpanishPairEnd(const std::string &text,
                                       std::size_t begin,
                                       std::size_t candidate,
                                       std::size_t hardLimit) {
  std::size_t questionOpen = std::string::npos;
  std::size_t exclamationOpen = std::string::npos;

  for (std::size_t i = begin; i < candidate; ++i) {
    if (isOpeningSpanishQuestion(text, i)) {
      questionOpen = i;
      ++i;
      continue;
    }

    if (isOpeningSpanishExclamation(text, i)) {
      exclamationOpen = i;
      ++i;
      continue;
    }

    if (isClosingSpanishQuestion(text, i)) {
      questionOpen = std::string::npos;
    }

    if (isClosingSpanishExclamation(text, i)) {
      exclamationOpen = std::string::npos;
    }
  }

  if (questionOpen != std::string::npos) {
    for (std::size_t i = candidate; i < hardLimit; ++i) {
      if (isClosingSpanishQuestion(text, i)) {
        return skipClosingMarksForward(text, i + 1, hardLimit);
      }
    }
  }

  if (exclamationOpen != std::string::npos) {
    for (std::size_t i = candidate; i < hardLimit; ++i) {
      if (isClosingSpanishExclamation(text, i)) {
        return skipClosingMarksForward(text, i + 1, hardLimit);
      }
    }
  }

  return std::string::npos;
}

} // namespace

std::size_t chooseSmartSplitPoint(const std::string &text, std::size_t offset,
                                  std::size_t preferredBytes) {
  const std::size_t remaining = text.size() - offset;
  if (remaining <= preferredBytes) {
    return text.size();
  }

  const std::size_t candidate =
      clampToUtf8Boundary(text, std::min(offset + preferredBytes, text.size()));
  const std::size_t hardLimit = clampToUtf8Boundary(
      text, std::min(offset + (preferredBytes * 2), text.size()));
  const std::size_t minBoundary = offset + std::max<std::size_t>(1, preferredBytes / 3);

  // Spanish questions/exclamations should be kept intact when the closing mark
  // is reasonably close. This avoids chunks like "¿Cuánto cuesta" + "...?".
  auto spanishPairEnd =
      findUnclosedSpanishPairEnd(text, offset, candidate, hardLimit);
  if (spanishPairEnd != std::string::npos && spanishPairEnd > offset) {
    return spanishPairEnd;
  }

  auto paragraph =
      findLastParagraphBoundary(text, offset, candidate, minBoundary);
  if (paragraph != std::string::npos) {
    return paragraph;
  }

  auto sentence =
      findLastStrongSentenceBoundary(text, offset, candidate, minBoundary);
  if (sentence != std::string::npos) {
    return sentence;
  }

  auto soft = findLastSoftBoundary(text, offset, candidate, minBoundary);
  if (soft != std::string::npos) {
    return soft;
  }

  auto whitespace =
      findLastWhitespaceBoundary(text, offset, candidate, minBoundary);
  if (whitespace != std::string::npos) {
    return whitespace;
  }

  // If a normal word crosses the preferred limit, allow a little overrun and
  // cut at the next whitespace. If no whitespace exists, cut at a UTF-8 safe
  // hard boundary to protect RAM/CPU.
  auto forwardWhitespace = findForwardWhitespaceBoundary(text, candidate, hardLimit);
  if (forwardWhitespace != std::string::npos) {
    return forwardWhitespace;
  }

  return hardLimit > offset ? hardLimit : candidate;
}

} // namespace piper::text
