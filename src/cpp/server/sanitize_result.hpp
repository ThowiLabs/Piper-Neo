#ifndef PIPER_SERVER_SANITIZE_RESULT_H_
#define PIPER_SERVER_SANITIZE_RESULT_H_

#include <cstddef>
#include <string>
#include <vector>

namespace piper_server {

struct TtsTextSanitizeResult {
  bool ok = true;
  std::string speakText;
  std::vector<std::string> warnings;
  double riskScore = 0.0;
  std::size_t rawBytes = 0;
  std::size_t speakBytes = 0;
  std::size_t rawChars = 0;
  std::size_t speakChars = 0;
  std::size_t urls = 0;
  std::size_t emails = 0;
  std::size_t codeBlocks = 0;
  std::size_t emojis = 0;
};

void addTtsSanitizeWarning(TtsTextSanitizeResult &result, const std::string &warning);

} // namespace piper_server

#endif // PIPER_SERVER_SANITIZE_RESULT_H_
