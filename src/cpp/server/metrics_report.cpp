#include "metrics_report.hpp"

namespace piper_server {

json resourcePolicyJson(const ServerOptions &options, const FairTtsScheduler &scheduler) {
  return json{{"mode", "auto"},
              {"profile", options.cpuProfile},
              {"hardware_threads", options.resourcePolicy.hardwareThreads},
              {"memory_bytes", options.resourcePolicy.memoryBytes},
              {"cpu_threads_per_worker", options.cpuThreads.value_or(0)},
              {"max_concurrent_jobs", options.maxConcurrentJobs},
              {"chunk_workers", scheduler.workerTotal()},
              {"max_model_replicas", options.maxModelReplicas},
              {"queue_size", options.queueSize},
              {"queue_timeout_seconds", options.queueTimeoutSeconds},
              {"max_temp_bytes", options.maxTempBytes},
              {"active_jobs", scheduler.activeJobCount()},
              {"waiting_jobs", scheduler.waitingJobCount()}};
}

json metricsJson(const ServerMetrics &metrics, const FairTtsScheduler &scheduler,
                 const ServerOptions &options) {
  return json{{"jobs",
               json{{"active", scheduler.activeJobCount()},
                    {"waiting", scheduler.waitingJobCount()},
                    {"accepted", metrics.acceptedJobs.load()},
                    {"completed", metrics.completedJobs.load()},
                    {"cancelled", metrics.cancelledJobs.load()},
                    {"failed", metrics.failedJobs.load()},
                    {"rejected", metrics.rejectedJobs.load()}}},
              {"chunks",
               json{{"processing", metrics.processingChunks.load()},
                    {"completed", metrics.completedChunks.load()},
                    {"failed", metrics.failedChunks.load()}}},
              {"storage", json{{"temp_bytes", metrics.tempStorageBytes.load()},
                                  {"max_temp_bytes", options.maxTempBytes},
                                  {"output_retention_seconds", options.outputRetentionSeconds}}},
              {"resources", resourcePolicyJson(options, scheduler)},
              {"text_preprocessing",
               json{{"sanitized_inputs", metrics.sanitizedInputs.load()},
                    {"sanitize_warnings", metrics.sanitizeWarnings.load()},
                    {"rejected_inputs", metrics.rejectedTextInputs.load()}}}};
}

} // namespace piper_server
