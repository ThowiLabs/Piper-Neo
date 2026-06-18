#ifndef PIPER_SERVER_TEXT_SANITIZER_H_
#define PIPER_SERVER_TEXT_SANITIZER_H_

#include <cstddef>
#include <string>

#include "../json.hpp"
#include "sanitize_result.hpp"

namespace piper_server {

using json = nlohmann::json;

std::string sanitizeTtsTextForApi(const std::string &rawText, std::size_t maxChars,
                                  TtsTextSanitizeResult &result);
json ttsSanitizeResultToJson(const TtsTextSanitizeResult &result);

} // namespace piper_server

#endif // PIPER_SERVER_TEXT_SANITIZER_H_
