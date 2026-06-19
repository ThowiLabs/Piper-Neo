#ifndef PIPER_APP_SYNTHESIS_OUTPUT_HPP_
#define PIPER_APP_SYNTHESIS_OUTPUT_HPP_

#include <filesystem>
#include <optional>
#include <string>

#include "run_config.hpp"
#include "../piper.hpp"

namespace piper_app {

void logSynthesisResult(const piper::SynthesisResult &result);
void writeLineOutput(piper::PiperConfig &piperConfig, piper::Voice &voice,
                     const RunConfig &runConfig, const std::string &line,
                     OutputType outputType,
                     const std::optional<std::filesystem::path> &maybeOutputPath,
                     piper::SynthesisResult &result);

} // namespace piper_app

#endif // PIPER_APP_SYNTHESIS_OUTPUT_HPP_
