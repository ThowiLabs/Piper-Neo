#ifndef PIPER_SERVER_MARKUP_MARKUP_PARSER_H_
#define PIPER_SERVER_MARKUP_MARKUP_PARSER_H_

#include <cstdint>
#include <map>
#include <optional>
#include <string>

#include "../types.hpp"

namespace piper_server {

bool looksLikeMarkupTts(const std::string &text);
std::map<std::string, std::string> parseLooseTagAttributes(const std::string &tagBody);
std::optional<float> parseMarkupFloat(const std::string &value, const std::string &field,
                                      float minValue, float maxValue);
std::optional<piper::SpeakerId> parseMarkupSpeaker(const std::string &value);
std::optional<std::uint64_t> parseSilenceDurationMs(const std::map<std::string, std::string> &attrs);
MarkupVoiceSettings parseMarkupModelSettings(const std::map<std::string, std::string> &attrs);
MarkupParseResult parseMarkupScript(const std::string &text);

} // namespace piper_server

#endif // PIPER_SERVER_MARKUP_MARKUP_PARSER_H_
