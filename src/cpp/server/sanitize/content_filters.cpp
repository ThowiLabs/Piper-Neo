#include "content_filters.hpp"

#include <regex>
#include <string>

namespace piper_server::sanitize {
namespace {

std::string regexReplaceWithWarning(const std::string &input, const std::regex &pattern,
                                    const std::string &replacement,
                                    TtsTextSanitizeResult &result,
                                    const std::string &warning) {
  if (!std::regex_search(input, pattern)) return input;
  addTtsSanitizeWarning(result, warning);
  return std::regex_replace(input, pattern, replacement);
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
  return !input.empty() && (static_cast<double>(structural) / static_cast<double>(input.size())) > 0.06;
}

} // namespace

std::string stripMarkupForTts(const std::string &input, TtsTextSanitizeResult &result) {
  static const std::regex brRegex(R"(<\s*br\s*\/?\s*>)", std::regex::icase);
  static const std::regex blockTagRegex(
      R"(<\s*\/?\s*(p|div|section|article|main|header|footer|li|ul|ol|h[1-6]|blockquote|tr|td|th|table)\b[^>]*>)",
      std::regex::icase);
  static const std::regex tagRegex(R"(<[^>]+>)");

  auto text = std::regex_replace(input, brRegex, "\n");
  text = std::regex_replace(text, blockTagRegex, "\n");
  text = std::regex_replace(text, tagRegex, " ");
  if (text != input) addTtsSanitizeWarning(result, "MARKUP_STRIPPED");
  return text;
}

std::string stripBbcodeForTts(const std::string &input, TtsTextSanitizeResult &result) {
  static const std::regex bbcodeRegex(
      R"(\[(\/?)(b|i|u|s|url|img|quote|code|color|size|list|\*|center|left|right)(=[^\]]*)?\])",
      std::regex::icase);
  return regexReplaceWithWarning(input, bbcodeRegex, " ", result, "BBCODE_STRIPPED");
}

std::string summarizeFencedCodeForTts(const std::string &input, TtsTextSanitizeResult &result) {
  static const std::regex fenceRegex(R"((```[\s\S]*?```|~~~[\s\S]*?~~~))");
  if (!std::regex_search(input, fenceRegex)) return input;
  ++result.codeBlocks;
  addTtsSanitizeWarning(result, "CODE_SUMMARIZED");
  return std::regex_replace(input, fenceRegex, " bloque de codigo omitido ");
}

std::string stripMarkdownForTts(const std::string &input, TtsTextSanitizeResult &result) {
  static const std::regex markdownLinkRegex(R"(!\[([^\]]*)\]\([^)]*\)|\[([^\]]+)\]\(([^)]+)\))");
  static const std::regex markdownSyntaxRegex(
      R"((^|\n)\s{0,3}#{1,6}\s+|(^|\n)\s*[-*+]\s+|(^|\n)\s*\d+[.)]\s+|(^|\n)\s*>\s?|[\*_~#]{1,}|[|]{2,})");

  auto text = std::regex_replace(input, markdownLinkRegex, "$1$2");
  return regexReplaceWithWarning(text, markdownSyntaxRegex, " ", result, "MARKDOWN_STRIPPED");
}

std::string summarizeCodeSignalsForTts(const std::string &input, TtsTextSanitizeResult &result) {
  if (!looksLikeCodeForTts(input)) return input;
  static const std::regex codeLineRegex(
      R"((\b(function|const|let|var|class|return|import|export|SELECT|INSERT|UPDATE|DELETE|FROM|WHERE)\b[^\n]{0,240}|[{\[\];<>_=]{2,}))",
      std::regex::icase);
  ++result.codeBlocks;
  addTtsSanitizeWarning(result, "CODE_SUMMARIZED");
  return std::regex_replace(input, codeLineRegex, " fragmento tecnico omitido ");
}

std::string summarizeHighEntropyForTts(const std::string &input, TtsTextSanitizeResult &result) {
  static const std::regex highEntropy(R"(\b[A-Za-z0-9+\/_=-]{80,}\b)");
  if (!std::regex_search(input, highEntropy)) return input;
  addTtsSanitizeWarning(result, "HIGH_ENTROPY_SPAN");
  return std::regex_replace(input, highEntropy, " cadena tecnica omitida ");
}

void countUrlsAndEmails(const std::string &input, TtsTextSanitizeResult &result) {
  // La API no convierte URLs ni correos a frases como "enlace a" o "correo".
  // La pronunciación queda a cargo de neo.text_normalization por modelo.
  static const std::regex urlRegex(R"(\b(?:https?:\/\/|www\.)\S+|\b[A-Z0-9.-]+\.[A-Z]{2,}\b)",
                                  std::regex::icase);
  static const std::regex emailRegex(R"(\b[A-Z0-9._%+-]+@[A-Z0-9.-]+\.[A-Z]{2,}\b)",
                                    std::regex::icase);
  result.urls += static_cast<std::size_t>(std::distance(
      std::sregex_iterator(input.begin(), input.end(), urlRegex), std::sregex_iterator()));
  result.emails += static_cast<std::size_t>(std::distance(
      std::sregex_iterator(input.begin(), input.end(), emailRegex), std::sregex_iterator()));
}

} // namespace piper_server::sanitize
