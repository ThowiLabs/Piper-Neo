#ifndef PIPER_APP_SYNTHESIS_INPUT_HPP_
#define PIPER_APP_SYNTHESIS_INPUT_HPP_

#include <istream>
#include <memory>

#include "run_config.hpp"

namespace piper_app {

std::unique_ptr<std::istream> openDirectInput(const RunConfig &runConfig,
                                              std::istream *&inputStream);
bool shouldUseDirectStreamMode(const RunConfig &runConfig);

} // namespace piper_app

#endif // PIPER_APP_SYNTHESIS_INPUT_HPP_
