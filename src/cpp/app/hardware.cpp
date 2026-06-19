#include "hardware.hpp"

#include "hardware_probe.hpp"
#include "resource_policy.hpp"

#include <cstdint>

#include <spdlog/spdlog.h>

namespace piper_app {

void applyAutoServerResourceConfig(RunConfig &runConfig) {
  const unsigned int hardwareThreads = detectAvailableHardwareThreads();
  const uint64_t memoryBytes = detectAvailableMemoryBytes();

  applyServerResourcePolicy(runConfig, hardwareThreads, memoryBytes);

  spdlog::info("Server resource policy: profile={} detected_threads={} detected_memory_mb={} cpu_threads={} chunk_workers={} max_jobs={} max_model_replicas={} queue_size={} max_temp_bytes={}",
               runConfig.cpuProfile, runConfig.detectedHardwareThreads,
               runConfig.detectedMemoryBytes == 0 ? 0 : runConfig.detectedMemoryBytes / (1024ULL * 1024ULL),
               runConfig.cpuThreads.value_or(0), runConfig.chunkWorkers,
               runConfig.maxConcurrentJobs, runConfig.maxModelReplicas,
               runConfig.queueSize, runConfig.maxTempBytes);
}

} // namespace piper_app
