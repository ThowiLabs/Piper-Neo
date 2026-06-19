#ifndef PIPER_SERVER_ROUTES_TTS_REQUEST_HPP_
#define PIPER_SERVER_ROUTES_TTS_REQUEST_HPP_

#include <cstdint>
#include <optional>
#include <string>
#include "server/http_types.hpp"

namespace piper_server {

struct TtsRouteRequest {
  std::string text;
  std::optional<std::string> requestedModel;
  std::optional<std::int64_t> speakerId;
  std::optional<float> noiseScale;
  std::optional<float> lengthScale;
  std::optional<float> noiseW;
  std::optional<float> sentenceSilenceSeconds;
};

struct TtsRouteParseResult {
  bool ok = false;
  int status = 400;
  json response;
  TtsRouteRequest value;
};

TtsRouteParseResult parseTtsRouteRequest(const HttpRequest &request,
                                         std::size_t maxInputBytes);

} // namespace piper_server

#endif // PIPER_SERVER_ROUTES_TTS_REQUEST_HPP_
