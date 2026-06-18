#ifndef PIPER_SERVER_METRICS_REPORT_H_
#define PIPER_SERVER_METRICS_REPORT_H_

#include "tts_scheduler.hpp"
#include "types.hpp"

namespace piper_server {

json resourcePolicyJson(const ServerOptions &options, const FairTtsScheduler &scheduler);
json metricsJson(const ServerMetrics &metrics, const FairTtsScheduler &scheduler,
                 const ServerOptions &options);

} // namespace piper_server

#endif // PIPER_SERVER_METRICS_REPORT_H_
