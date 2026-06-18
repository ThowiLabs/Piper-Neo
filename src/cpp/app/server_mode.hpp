#ifndef PIPER_APP_SERVER_MODE_HPP_
#define PIPER_APP_SERVER_MODE_HPP_

#include "run_config.hpp"
#include "../piper.hpp"

namespace piper_app {

int runServerMode(piper::PiperConfig &piperConfig, piper::Voice &voice,
                  const RunConfig &runConfig);

} // namespace piper_app

#endif // PIPER_APP_SERVER_MODE_HPP_
