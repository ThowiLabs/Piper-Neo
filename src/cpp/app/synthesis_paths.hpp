#ifndef PIPER_APP_SYNTHESIS_PATHS_HPP_
#define PIPER_APP_SYNTHESIS_PATHS_HPP_

#include <filesystem>

#include "run_config.hpp"

namespace piper_app {

std::filesystem::path timestampedOutputPath(const RunConfig &runConfig);

} // namespace piper_app

#endif // PIPER_APP_SYNTHESIS_PATHS_HPP_
