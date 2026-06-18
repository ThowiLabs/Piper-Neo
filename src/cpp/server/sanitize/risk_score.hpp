#ifndef PIPER_SERVER_SANITIZE_RISK_SCORE_H_
#define PIPER_SERVER_SANITIZE_RISK_SCORE_H_

#include "../sanitize_result.hpp"

namespace piper_server::sanitize {

double calculateRiskScore(const TtsTextSanitizeResult &result);

} // namespace piper_server::sanitize

#endif // PIPER_SERVER_SANITIZE_RISK_SCORE_H_
