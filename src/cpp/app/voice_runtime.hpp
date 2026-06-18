#ifndef PIPER_APP_VOICE_RUNTIME_HPP_
#define PIPER_APP_VOICE_RUNTIME_HPP_

#include <optional>

#include "run_config.hpp"
#include "../neo_model.hpp"
#include "../piper.hpp"

namespace piper_app {

void prepareVoiceRuntime(const RunConfig &runConfig, const char *argv0,
                         piper::PiperConfig &piperConfig, piper::Voice &voice,
                         std::optional<piper_neo::ExtractedNeoModel> &extractedNeoModel);

void applySynthesisOverrides(const RunConfig &runConfig, piper::Voice &voice);

} // namespace piper_app

#endif // PIPER_APP_VOICE_RUNTIME_HPP_
