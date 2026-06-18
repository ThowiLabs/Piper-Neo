#include "risk_score.hpp"

#include <algorithm>

namespace piper_server::sanitize {

double calculateRiskScore(const TtsTextSanitizeResult &result) {
  double score = 0.0;
  for (const auto &warning : result.warnings) {
    if (warning == "EMPTY_AFTER_SANITIZE" || warning == "TEXT_TOO_LONG" || warning == "INVALID_UTF8") {
      score += 0.45;
    } else if (warning == "CODE_SUMMARIZED" || warning == "HIGH_ENTROPY_SPAN") {
      score += 0.28;
    } else if (warning == "INVISIBLE_REMOVED") {
      score += 0.14;
    } else {
      score += 0.06;
    }
  }
  return std::min(1.0, score);
}

} // namespace piper_server::sanitize
