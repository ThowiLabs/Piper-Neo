#include "text_sanitizer.hpp"

#include <string>

#include "sanitize/content_filters.hpp"
#include "sanitize/risk_score.hpp"
#include "sanitize/utf8_text.hpp"
#include "utils.hpp"

namespace piper_server {

std::string sanitizeTtsTextForApi(const std::string &rawText, std::size_t maxChars,
                                  TtsTextSanitizeResult &result) {
  result.rawBytes = rawText.size();
  std::size_t rawChars = 0;
  if (!sanitize::isValidUtf8Strict(rawText, &rawChars)) {
    result.ok = false;
    addTtsSanitizeWarning(result, "INVALID_UTF8");
    result.riskScore = 1.0;
    return {};
  }
  result.rawChars = rawChars;

  std::string text = trimCopy(rawText);
  if (text != rawText) addTtsSanitizeWarning(result, "TEXT_TRIMMED");

  text = sanitize::normalizeUtf8ForTts(text, result);
  if (!result.ok) return {};

  text = sanitize::stripMarkupForTts(text, result);
  text = sanitize::stripBbcodeForTts(text, result);
  text = sanitize::summarizeFencedCodeForTts(text, result);
  text = sanitize::stripMarkdownForTts(text, result);
  text = sanitize::summarizeCodeSignalsForTts(text, result);

  sanitize::countUrlsAndEmails(text, result);
  text = sanitize::summarizeHighEntropyForTts(text, result);
  text = sanitize::collapseRepeatedCodepoints(text, 5, result);
  text = sanitize::normalizeWhitespaceForTts(text, result);

  std::size_t speakChars = 0;
  sanitize::isValidUtf8Strict(text, &speakChars);
  if (speakChars > maxChars) {
    text = trimCopy(sanitize::utf8PrefixByChars(text, maxChars));
    addTtsSanitizeWarning(result, "TEXT_TOO_LONG");
  }

  if (text.empty()) {
    addTtsSanitizeWarning(result, "EMPTY_AFTER_SANITIZE");
    result.ok = false;
    result.riskScore = 1.0;
    return {};
  }

  result.speakText = text;
  result.speakBytes = text.size();
  sanitize::isValidUtf8Strict(text, &result.speakChars);
  result.riskScore = sanitize::calculateRiskScore(result);
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
