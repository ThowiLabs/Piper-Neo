#ifndef PIPER_SERVER_MARKUP_TTS_H_
#define PIPER_SERVER_MARKUP_TTS_H_

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include <map>

#include "../server.hpp"
#include "tts_scheduler.hpp"
#include "types.hpp"

namespace piper_server {

std::optional<float> requestFloatOption(const json &input, const std::vector<std::string> &keys,
                                        const std::string &field, float minValue,
                                        float maxValue);

bool looksLikeMarkupTts(const std::string &text);
MarkupVoiceSettings parseMarkupModelSettings(const std::map<std::string, std::string> &attrs);
MarkupParseResult parseMarkupScript(const std::string &text);
MarkupRenderResult synthesizeMarkupScript(const std::string &rawText, const std::string &fileName,
                                          const std::filesystem::path &outputPath,
                                          const std::optional<std::string> &defaultModel,
                                          const std::optional<piper::SpeakerId> &defaultSpeakerId,
                                          const std::optional<float> &defaultNoiseScale,
                                          const std::optional<float> &defaultLengthScale,
                                          const std::optional<float> &defaultNoiseW,
                                          const std::optional<float> &defaultSentenceSilence,
                                          const ServerOptions &options,
                                          FairTtsScheduler &scheduler,
                                          const std::function<bool()> &shouldCancel);

} // namespace piper_server

#endif // PIPER_SERVER_MARKUP_TTS_H_
