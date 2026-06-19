#ifndef PIPER_SERVER_ROUTES_TTS_PAYLOAD_HPP_
#define PIPER_SERVER_ROUTES_TTS_PAYLOAD_HPP_

#include <optional>
#include <string>

#include "server/types.hpp"

namespace piper_server {

json ttsSuccessPayload(const std::string &fileName, const TtsJobResult &result,
                       const json &markup, const json &textPreprocessing,
                       const std::optional<std::string> &fallbackModel = std::nullopt);

} // namespace piper_server

#endif // PIPER_SERVER_ROUTES_TTS_PAYLOAD_HPP_
