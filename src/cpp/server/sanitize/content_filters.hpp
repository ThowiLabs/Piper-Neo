#ifndef PIPER_SERVER_SANITIZE_CONTENT_FILTERS_H_
#define PIPER_SERVER_SANITIZE_CONTENT_FILTERS_H_

#include <string>

#include "../sanitize_result.hpp"

namespace piper_server::sanitize {

std::string stripMarkupForTts(const std::string &input, TtsTextSanitizeResult &result);
std::string stripBbcodeForTts(const std::string &input, TtsTextSanitizeResult &result);
std::string summarizeFencedCodeForTts(const std::string &input, TtsTextSanitizeResult &result);
std::string stripMarkdownForTts(const std::string &input, TtsTextSanitizeResult &result);
std::string summarizeCodeSignalsForTts(const std::string &input, TtsTextSanitizeResult &result);
std::string summarizeHighEntropyForTts(const std::string &input, TtsTextSanitizeResult &result);
void countUrlsAndEmails(const std::string &input, TtsTextSanitizeResult &result);

} // namespace piper_server::sanitize

#endif // PIPER_SERVER_SANITIZE_CONTENT_FILTERS_H_
