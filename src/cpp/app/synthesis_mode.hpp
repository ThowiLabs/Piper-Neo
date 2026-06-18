#ifndef PIPER_APP_SYNTHESIS_MODE_HPP_
#define PIPER_APP_SYNTHESIS_MODE_HPP_

#include "run_config.hpp"
#include "../piper.hpp"

namespace piper_app {

int runSynthesisMode(piper::PiperConfig &piperConfig, piper::Voice &voice,
                     RunConfig &runConfig);

} // namespace piper_app

#endif // PIPER_APP_SYNTHESIS_MODE_HPP_
