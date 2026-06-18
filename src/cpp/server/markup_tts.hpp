#ifndef PIPER_SERVER_MARKUP_TTS_H_
#define PIPER_SERVER_MARKUP_TTS_H_

#include <filesystem>
#include <functional>
#include <optional>
#include <string>

#include "../server.hpp"
#include "markup/markup_parser.hpp"
#include "markup/request_options.hpp"
#include "tts_scheduler.hpp"
#include "types.hpp"

namespace piper_server {

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
