#ifndef PIPER_APP_CLI_VALIDATION_HPP_
#define PIPER_APP_CLI_VALIDATION_HPP_

#include "run_config.hpp"

#include <filesystem>
#include <optional>

namespace piper_app {

void finalizeRunConfig(
    RunConfig &runConfig,
    const std::optional<std::filesystem::path> &modelConfigPathInput);

} // namespace piper_app

#endif // PIPER_APP_CLI_VALIDATION_HPP_
