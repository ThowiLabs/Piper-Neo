#include "sanitize_result.hpp"

#include <algorithm>

namespace piper_server {

void addTtsSanitizeWarning(TtsTextSanitizeResult &result, const std::string &warning) {
  if (std::find(result.warnings.begin(), result.warnings.end(), warning) == result.warnings.end()) {
    result.warnings.push_back(warning);
  }
}

} // namespace piper_server
