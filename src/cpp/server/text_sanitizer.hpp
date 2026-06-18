#ifndef PIPER_SERVER_TEXT_SANITIZER_H_
#define PIPER_SERVER_TEXT_SANITIZER_H_

#include <cstddef>
#include <string>

#include "types.hpp"

namespace piper_server {

std::string sanitizeTtsTextForApi(const std::string &rawText, std::size_t maxChars,
                                  TtsTextSanitizeResult &result);
json ttsSanitizeResultToJson(const TtsTextSanitizeResult &result);

} // namespace piper_server

#endif // PIPER_SERVER_TEXT_SANITIZER_H_
