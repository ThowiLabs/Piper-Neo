#include "text_sanitizer.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "utils.hpp"

namespace piper_server {
namespace {

void addTtsWarning(TtsTextSanitizeResult &result, const std::string &warning) {
  if (std::find(result.warnings.begin(), result.warnings.end(), warning) == result.warnings.end()) {
    result.warnings.push_back(warning);
  }
}

bool isUtf8Continuation(unsigned char byte) { return (byte & 0xC0) == 0x80; }

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

bool isValidUtf8Strict(const std::string &input, std::size_t *charCount = nullptr) {
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
  case 0xFB00: return 0; // handled as multi-char below
  case 0xFB01: return 0;
  case 0xFB02: return 0;
  default:
    return cp;
  }
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
      addTtsWarning(result, "INVALID_UTF8");
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

  if (changed) addTtsWarning(result, "NORMALIZED_UNICODE");
  if (controlsRemoved) addTtsWarning(result, "CONTROL_REMOVED");
  if (invisiblesRemoved) addTtsWarning(result, "INVISIBLE_REMOVED");
  if (emojiSeen) addTtsWarning(result, "EMOJI_SUMMARIZED");
  if (result.emojis > 3) out.append(" emojis omitidos ");
  return out;
}

std::string regexReplaceWithWarning(const std::string &input, const std::regex &pattern,
                                    const std::string &replacement,
                                    TtsTextSanitizeResult &result,
                                    const std::string &warning) {
  if (!std::regex_search(input, pattern)) return input;
  addTtsWarning(result, warning);
  return std::regex_replace(input, pattern, replacement);
}

std::string summarizeUrlsForTts(const std::string &input, TtsTextSanitizeResult &result) {
  // La API ya no reescribe URLs ni dominios a texto tipo "enlace a ...".
  // La pronunciacion de URLs, dominios y correos queda bajo control de
  // neo.text_normalization de cada modelo. Esto evita falsos positivos como
  // comandos: apt install docker.io.
  (void)result;
  return input;
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

  if (changed) addTtsWarning(result, "REPETITIONS_COLLAPSED");
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
  if (changed) addTtsWarning(result, "WHITESPACE_NORMALIZED");
  return trimmed;
}

bool hasHighEntropySpan(const std::string &input) {
  static const std::regex highEntropy(R"(\b[A-Za-z0-9+\/_=-]{80,}\b)");
  return std::regex_search(input, highEntropy);
}

bool looksLikeCodeForTts(const std::string &input) {
  static const std::regex codeSignal(
      R"((```[\s\S]*?```|~~~[\s\S]*?~~~|\b(function|const|let|var|class|return|import|export|SELECT|INSERT|UPDATE|DELETE|FROM|WHERE)\b|[{\[\];<>_=]{2,}|=>|::))",
      std::regex::icase);
  if (!std::regex_search(input, codeSignal)) return false;

  std::size_t structural = 0;
  for (char c : input) {
    if (std::string("{}[]();=<>_\\/|$#").find(c) != std::string::npos) ++structural;
  }
  return input.size() > 0 && (static_cast<double>(structural) / static_cast<double>(input.size())) > 0.06;
}

} // namespace

std::string sanitizeTtsTextForApi(const std::string &rawText, std::size_t maxChars,
                                  TtsTextSanitizeResult &result) {
  result.rawBytes = rawText.size();
  std::size_t rawChars = 0;
  if (!isValidUtf8Strict(rawText, &rawChars)) {
    result.ok = false;
    addTtsWarning(result, "INVALID_UTF8");
    result.riskScore = 1.0;
    return {};
  }
  result.rawChars = rawChars;

  std::string text = trimCopy(rawText);
  if (text != rawText) addTtsWarning(result, "TEXT_TRIMMED");

  text = normalizeUtf8ForTts(text, result);
  if (!result.ok) return {};

  static const std::regex brRegex(R"(<\s*br\s*\/?\s*>)", std::regex::icase);
  static const std::regex blockTagRegex(R"(<\s*\/?\s*(p|div|section|article|main|header|footer|li|ul|ol|h[1-6]|blockquote|tr|td|th|table)\b[^>]*>)", std::regex::icase);
  static const std::regex tagRegex(R"(<[^>]+>)");
  const auto beforeMarkup = text;
  text = std::regex_replace(text, brRegex, "\n");
  text = std::regex_replace(text, blockTagRegex, "\n");
  text = std::regex_replace(text, tagRegex, " ");
  if (text != beforeMarkup) addTtsWarning(result, "MARKUP_STRIPPED");

  static const std::regex bbcodeRegex(R"(\[(\/?)(b|i|u|s|url|img|quote|code|color|size|list|\*|center|left|right)(=[^\]]*)?\])", std::regex::icase);
  text = regexReplaceWithWarning(text, bbcodeRegex, " ", result, "BBCODE_STRIPPED");

  static const std::regex fenceRegex(R"((```[\s\S]*?```|~~~[\s\S]*?~~~))");
  if (std::regex_search(text, fenceRegex)) {
    text = std::regex_replace(text, fenceRegex, " bloque de codigo omitido ");
    addTtsWarning(result, "CODE_SUMMARIZED");
    ++result.codeBlocks;
  }

  static const std::regex markdownLinkRegex(R"(!\[([^\]]*)\]\([^)]*\)|\[([^\]]+)\]\(([^)]+)\))");
  text = std::regex_replace(text, markdownLinkRegex, "$1$2");

  static const std::regex markdownSyntaxRegex(R"((^|\n)\s{0,3}#{1,6}\s+|(^|\n)\s*[-*+]\s+|(^|\n)\s*\d+[.)]\s+|(^|\n)\s*>\s?|[\*_~#]{1,}|[|]{2,})");
  text = regexReplaceWithWarning(text, markdownSyntaxRegex, " ", result, "MARKDOWN_STRIPPED");

  if (looksLikeCodeForTts(text)) {
    static const std::regex codeLineRegex(
        R"((\b(function|const|let|var|class|return|import|export|SELECT|INSERT|UPDATE|DELETE|FROM|WHERE)\b[^\n]{0,240}|[{\[\];<>_=]{2,}))",
        std::regex::icase);
    text = std::regex_replace(text, codeLineRegex, " fragmento tecnico omitido ");
    addTtsWarning(result, "CODE_SUMMARIZED");
    ++result.codeBlocks;
  }

  text = summarizeUrlsForTts(text, result);

  // No se reemplazan correos por "correo electronico" en la API.
  // Si un modelo necesita leer correos como "arroba" y "punto", debe
  // declararlo en neo.text_normalization.builtin.emails.
  static const std::regex emailRegex(R"(\b[A-Z0-9._%+-]+@[A-Z0-9.-]+\.[A-Z]{2,}\b)", std::regex::icase);
  if (std::regex_search(text, emailRegex)) {
    ++result.emails;
  }

  if (hasHighEntropySpan(text)) {
    static const std::regex highEntropy(R"(\b[A-Za-z0-9+\/_=-]{80,}\b)");
    text = std::regex_replace(text, highEntropy, " cadena tecnica omitida ");
    addTtsWarning(result, "HIGH_ENTROPY_SPAN");
  }

  text = collapseRepeatedCodepoints(text, 5, result);
  text = normalizeWhitespaceForTts(text, result);

  std::size_t speakChars = 0;
  isValidUtf8Strict(text, &speakChars);
  if (speakChars > maxChars) {
    std::string clipped;
    std::size_t count = 0;
    for (std::size_t i = 0; i < text.size() && count < maxChars;) {
      std::uint32_t cp = 0;
      std::size_t width = 0;
      if (!decodeUtf8At(text, i, cp, width)) break;
      clipped.append(text.substr(i, width));
      i += width;
      ++count;
    }
    text = trimCopy(clipped);
    addTtsWarning(result, "TEXT_TOO_LONG");
  }

  if (text.empty()) {
    addTtsWarning(result, "EMPTY_AFTER_SANITIZE");
    result.ok = false;
    result.riskScore = 1.0;
    return {};
  }

  result.speakText = text;
  result.speakBytes = text.size();
  isValidUtf8Strict(text, &result.speakChars);

  for (const auto &warning : result.warnings) {
    if (warning == "EMPTY_AFTER_SANITIZE" || warning == "TEXT_TOO_LONG" || warning == "INVALID_UTF8") {
      result.riskScore += 0.45;
    } else if (warning == "CODE_SUMMARIZED" || warning == "HIGH_ENTROPY_SPAN") {
      result.riskScore += 0.28;
    } else if (warning == "INVISIBLE_REMOVED") {
      result.riskScore += 0.14;
    } else {
      result.riskScore += 0.06;
    }
  }
  result.riskScore = std::min(1.0, result.riskScore);
  return result.speakText;
}

json ttsSanitizeResultToJson(const TtsTextSanitizeResult &result) {
  return json{{"speakText", result.speakText},
              {"warnings", result.warnings},
              {"riskScore", result.riskScore},
              {"stats", json{{"rawChars", result.rawChars},
                               {"speakChars", result.speakChars},
                               {"rawBytes", result.rawBytes},
                               {"speakBytes", result.speakBytes},
                               {"urls", result.urls},
                               {"emails", result.emails},
                               {"codeBlocks", result.codeBlocks},
                               {"emojis", result.emojis}}}};
}

} // namespace piper_server
