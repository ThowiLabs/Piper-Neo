#ifndef PIPER_SERVER_MODEL_LOADER_H_
#define PIPER_SERVER_MODEL_LOADER_H_

#include <memory>

#include "../server.hpp"
#include "piper/types.hpp"

namespace piper_server {

std::unique_ptr<piper::Voice> loadModelVoice(piper::PiperConfig &piperConfig,
                                             const ServerOptions &options,
                                             const ModelInfo &info);

} // namespace piper_server

#endif // PIPER_SERVER_MODEL_LOADER_H_
