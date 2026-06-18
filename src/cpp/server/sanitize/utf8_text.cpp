#include "utf8_text.hpp"

#include <cstdint>
#include <string>

#include "../utils.hpp"

namespace piper_server::sanitize {
namespace {

bool isUtf8Continuation(unsigned char byte) { return (byte & 0xC0) == 0x80; }

bool isDefaultIgnorableForTts(std::uint32_t cp) {
  return cp == 0x00AD || cp == 0x034F || cp == 0x061C || cp == 0x115F || cp == 0x1160 ||
         cp == 0x17B4 || cp == 0x17B5 || cp == 0x180E || cp == 0x200B || cp == 0x200C ||
         cp == 0x200D || cp == 0x200E || cp == 0x200F || cp == 0x202A || cp == 0x202B ||
         cp == 0x202C || cp == 0x202D || cp == 0x202E || cp == 0x2060 ||
         (cp >= 0x2061 && cp <= 0x206F) || cp == 0x3164 || cp == 0xFEFF || cp == 0xFFA0 ||
         (cp >= 0xFE00 && cp <= 0xFE0F);
}

bool isEmojiLike(std::uint32_t cp) {
  return (cp >= 0x1F000 && cp <= 0x1FAFF) || (cp >= 0x2600 && cp <= 0x27BF);
}

std::uint32_t compatibilityFoldCodepoint(std::uint32_t cp) {
  if (cp >= 0xFF01 && cp <= 0xFF5E) {
    return cp - 0xFEE0; // full-width ASCII
  }
  switch (cp) {
  case 0x00A0: return ' ';
  case 0x2018:
  case 0x2019:
  case 0x201B:
  case 0x2032:
    return '\'';
  case 0x201C:
  case 0x201D:
  case 0x2033:
    return '"';
  case 0x2010:
  case 0x2011:
  case 0x2012:
  case 0x2013:
  case 0x2014:
  case 0x2212:
    return '-';
  case 0x2026:
    return '.';
  case 0xFB00:
  case 0xFB01:
  case 0xFB02:
    return 0; // handled as multi-char by normalizeUtf8ForTts.
  default:
    return cp;
  }
}

} // namespace

bool decodeUtf8At(const std::string &input, std::size_t index, std::uint32_t &cp,
                  std::size_t &width) {
  if (index >= input.size()) return false;
  const auto b0 = static_cast<unsigned char>(input[index]);
  if (b0 <= 0x7F) {
    cp = b0;
    width = 1;
    return true;
  }

  if (b0 >= 0xC2 && b0 <= 0xDF) {
    if ((index + 1) >= input.size()) return false;
    const auto b1 = static_cast<unsigned char>(input[index + 1]);
    if (!isUtf8Continuation(b1)) return false;
    cp = ((b0 & 0x1F) << 6) | (b1 & 0x3F);
    width = 2;
    return true;
  }

  if (b0 >= 0xE0 && b0 <= 0xEF) {
    if ((index + 2) >= input.size()) return false;
    const auto b1 = static_cast<unsigned char>(input[index + 1]);
    const auto b2 = static_cast<unsigned char>(input[index + 2]);
    if (!isUtf8Continuation(b1) || !isUtf8Continuation(b2)) return false;
    if (b0 == 0xE0 && b1 < 0xA0) return false;
    if (b0 == 0xED && b1 >= 0xA0) return false; // surrogate range
    cp = ((b0 & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);
    width = 3;
    return true;
  }

  if (b0 >= 0xF0 && b0 <= 0xF4) {
    if ((index + 3) >= input.size()) return false;
    const auto b1 = static_cast<unsigned char>(input[index + 1]);
    const auto b2 = static_cast<unsigned char>(input[index + 2]);
    const auto b3 = static_cast<unsigned char>(input[index + 3]);
    if (!isUtf8Continuation(b1) || !isUtf8Continuation(b2) || !isUtf8Continuation(b3)) return false;
    if (b0 == 0xF0 && b1 < 0x90) return false;
    if (b0 == 0xF4 && b1 > 0x8F) return false;
    cp = ((b0 & 0x07) << 18) | ((b1 & 0x3F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F);
    width = 4;
    return true;
  }

  return false;
}

void appendUtf8(std::string &out, std::uint32_t cp) {
  if (cp <= 0x7F) {
    out.push_back(static_cast<char>(cp));
  } else if (cp <= 0x7FF) {
    out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else if (cp <= 0xFFFF) {
    out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else if (cp <= 0x10FFFF) {
    out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  }
}

bool isValidUtf8Strict(const std::string &input, std::size_t *charCount) {
  std::size_t count = 0;
  for (std::size_t i = 0; i < input.size();) {
    std::uint32_t cp = 0;
    std::size_t width = 0;
    if (!decodeUtf8At(input, i, cp, width)) return false;
    i += width;
    ++count;
  }
  if (charCount != nullptr) *charCount = count;
  return true;
}

std::string normalizeUtf8ForTts(const std::string &input, TtsTextSanitizeResult &result) {
  std::string out;
  bool changed = false;
  bool controlsRemoved = false;
  bool invisiblesRemoved = false;
  bool emojiSeen = false;
  std::size_t consecutiveEmoji = 0;

  for (std::size_t i = 0; i < input.size();) {
    std::uint32_t cp = 0;
    std::size_t width = 0;
    if (!decodeUtf8At(input, i, cp, width)) {
      result.ok = false;
      addTtsSanitizeWarning(result, "INVALID_UTF8");
      return {};
    }

    i += width;

    if (cp == 0xFEFF && out.empty()) {
      changed = true;
      invisiblesRemoved = true;
      continue;
    }

    if ((cp < 0x20 && cp != '\n' && cp != '\r' && cp != '\t') || (cp >= 0x7F && cp <= 0x9F)) {
      out.push_back(' ');
      changed = true;
      controlsRemoved = true;
      continue;
    }

    if (isDefaultIgnorableForTts(cp)) {
      changed = true;
      invisiblesRemoved = true;
      continue;
    }

    if (isEmojiLike(cp)) {
      emojiSeen = true;
      ++consecutiveEmoji;
      ++result.emojis;
      if (consecutiveEmoji <= 3) {
        out.append(" emoji ");
      }
      changed = true;
      continue;
    }
    consecutiveEmoji = 0;

    if (cp == 0xFB00) {
      out.append("ff");
      changed = true;
      continue;
    }
    if (cp == 0xFB01) {
      out.append("fi");
      changed = true;
      continue;
    }
    if (cp == 0xFB02) {
      out.append("fl");
      changed = true;
      continue;
    }

    auto folded = compatibilityFoldCodepoint(cp);
    if (folded != cp) changed = true;
    if (folded == '.') {
      out.append("...");
    } else {
      appendUtf8(out, folded);
    }
  }

  if (changed) addTtsSanitizeWarning(result, "NORMALIZED_UNICODE");
  if (controlsRemoved) addTtsSanitizeWarning(result, "CONTROL_REMOVED");
  if (invisiblesRemoved) addTtsSanitizeWarning(result, "INVISIBLE_REMOVED");
  if (emojiSeen) addTtsSanitizeWarning(result, "EMOJI_SUMMARIZED");
  if (result.emojis > 3) out.append(" emojis omitidos ");
  return out;
}

std::string collapseRepeatedCodepoints(const std::string &input, std::size_t maxRun,
                                       TtsTextSanitizeResult &result) {
  std::string out;
  std::uint32_t previous = 0;
  std::size_t run = 0;
  bool changed = false;

  for (std::size_t i = 0; i < input.size();) {
    std::uint32_t cp = 0;
    std::size_t width = 0;
    if (!decodeUtf8At(input, i, cp, width)) break;
    i += width;
    if (cp == previous) {
      ++run;
    } else {
      previous = cp;
      run = 1;
    }
    if (run <= maxRun) {
      appendUtf8(out, cp);
    } else {
      changed = true;
    }
  }

  if (changed) addTtsSanitizeWarning(result, "REPETITIONS_COLLAPSED");
  return out;
}

std::string normalizeWhitespaceForTts(const std::string &input, TtsTextSanitizeResult &result) {
  std::string out;
  out.reserve(input.size());
  bool changed = false;
  bool lastSpace = false;
  std::size_t newlines = 0;

  for (char c : input) {
    if (c == '\r' || c == '\n') {
      if (newlines < 2) out.push_back('\n');
      ++newlines;
      lastSpace = false;
      changed = true;
      continue;
    }
    newlines = 0;
    if (c == '\t' || c == '\f' || c == '\v' || c == ' ') {
      if (!lastSpace) out.push_back(' ');
      lastSpace = true;
      changed = true;
      continue;
    }
    out.push_back(c);
    lastSpace = false;
  }

  auto trimmed = trimCopy(out);
  if (trimmed != input) changed = true;
  if (changed) addTtsSanitizeWarning(result, "WHITESPACE_NORMALIZED");
  return trimmed;
}

std::string utf8PrefixByChars(const std::string &input, std::size_t maxChars) {
  std::string clipped;
  std::size_t count = 0;
  for (std::size_t i = 0; i < input.size() && count < maxChars;) {
    std::uint32_t cp = 0;
    std::size_t width = 0;
    if (!decodeUtf8At(input, i, cp, width)) break;
    clipped.append(input.substr(i, width));
    i += width;
    ++count;
  }
  return clipped;
}

} // namespace piper_server::sanitize
