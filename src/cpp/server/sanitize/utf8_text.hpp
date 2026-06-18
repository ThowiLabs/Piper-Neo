#ifndef PIPER_SERVER_SANITIZE_UTF8_TEXT_H_
#define PIPER_SERVER_SANITIZE_UTF8_TEXT_H_

#include <cstddef>
#include <cstdint>
#include <string>

#include "../sanitize_result.hpp"

namespace piper_server::sanitize {

bool decodeUtf8At(const std::string &input, std::size_t index, std::uint32_t &cp,
                  std::size_t &width);
void appendUtf8(std::string &out, std::uint32_t cp);
bool isValidUtf8Strict(const std::string &input, std::size_t *charCount = nullptr);

std::string normalizeUtf8ForTts(const std::string &input, TtsTextSanitizeResult &result);
std::string collapseRepeatedCodepoints(const std::string &input, std::size_t maxRun,
                                       TtsTextSanitizeResult &result);
std::string normalizeWhitespaceForTts(const std::string &input, TtsTextSanitizeResult &result);
std::string utf8PrefixByChars(const std::string &input, std::size_t maxChars);

} // namespace piper_server::sanitize

#endif // PIPER_SERVER_SANITIZE_UTF8_TEXT_H_
