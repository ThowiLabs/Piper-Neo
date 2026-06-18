#ifndef PIPER_SERVER_ROUTES_ROUTE_CONTEXT_H_
#define PIPER_SERVER_ROUTES_ROUTE_CONTEXT_H_

#include "../metrics_report.hpp"
#include "../model_registry.hpp"
#include "../tts_scheduler.hpp"

namespace piper_server {

struct RouteContext {
  const ServerOptions &options;
  ModelRegistry &modelRegistry;
  FairTtsScheduler &scheduler;
  ServerMetrics &metrics;
};

} // namespace piper_server

#endif // PIPER_SERVER_ROUTES_ROUTE_CONTEXT_H_
