#ifndef PIPER_SERVER_REQUEST_HANDLER_H_
#define PIPER_SERVER_REQUEST_HANDLER_H_

#include "http.hpp"
#include "model_cache.hpp"
#include "model_registry.hpp"
#include "tts_scheduler.hpp"

namespace piper_server {

void handleClient(SocketHandle clientSocket, const ServerOptions &options,
                  ModelCache &modelCache, ModelRegistry &modelRegistry,
                  FairTtsScheduler &scheduler, ServerMetrics &metrics);

} // namespace piper_server

#endif // PIPER_SERVER_REQUEST_HANDLER_H_
