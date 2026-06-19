#include "resource_policy.hpp"

#include "resource_limits.hpp"

#include <algorithm>
#include <cstdint>
#include <cstddef>

namespace piper_app {
namespace {

std::size_t clampSize(std::size_t value, std::size_t minValue,
                      std::size_t maxValue) {
  return std::max(minValue, std::min(value, maxValue));
}

} // namespace

void applyServerResourcePolicy(RunConfig &runConfig, unsigned int hardwareThreads,
                               uint64_t memoryBytes) {
  hardwareThreads = std::max(1u, hardwareThreads);
  runConfig.detectedHardwareThreads = hardwareThreads;
  runConfig.detectedMemoryBytes = memoryBytes;

  std::size_t autoCpuThreads = 1;
  std::size_t autoMaxJobs = 1;
  std::size_t autoChunkWorkers = 1;
  std::size_t autoReplicas = 1;
  std::size_t autoQueueSize = 16;

  if (runConfig.cpuProfile == "eco") {
    autoCpuThreads = 1;
    autoMaxJobs = hardwareThreads >= 4 ? 2 : 1;
    autoChunkWorkers = autoMaxJobs;
    autoReplicas = 1;
    autoQueueSize = 16;
  } else if (runConfig.cpuProfile == "balanced") {
    autoCpuThreads = hardwareThreads >= 6 ? 2 : 1;
    autoChunkWorkers = hardwareThreads >= 12 ? 4 : (hardwareThreads >= 6 ? 3 : (hardwareThreads >= 4 ? 2 : 1));
    autoMaxJobs = autoChunkWorkers;
    autoReplicas = hardwareThreads >= 12 ? 3 : (hardwareThreads >= 6 ? 2 : 1);
    autoQueueSize = 32;
  } else if (runConfig.cpuProfile == "fast") {
    autoCpuThreads = hardwareThreads >= 16 ? 3 : (hardwareThreads >= 4 ? 2 : 1);
    autoChunkWorkers = std::max<std::size_t>(1, hardwareThreads / autoCpuThreads);
    autoMaxJobs = std::min<std::size_t>(std::max<std::size_t>(2, autoChunkWorkers * 2), 16);
    autoReplicas = std::min<std::size_t>(autoMaxJobs, hardwareThreads >= 16 ? 5 : (hardwareThreads >= 8 ? 4 : 2));
    autoQueueSize = 64;
  } else if (runConfig.cpuProfile == "max") {
    autoCpuThreads = hardwareThreads >= 32 ? 4 : (hardwareThreads >= 12 ? 3 : (hardwareThreads >= 4 ? 2 : 1));
    autoChunkWorkers = std::max<std::size_t>(1, hardwareThreads / autoCpuThreads);
    autoMaxJobs = std::min<std::size_t>(std::max<std::size_t>(2, autoChunkWorkers * 2), 32);
    autoReplicas = std::min<std::size_t>(autoMaxJobs, std::max<std::size_t>(2, hardwareThreads / 2));
    autoQueueSize = 96;
  } else {
    // auto: default profile. Saturates the CPU budget exposed by the OS/container
    // while keeping per-worker ONNX threads and model replicas bounded by RAM.
    autoCpuThreads = hardwareThreads >= 32 ? 4 : (hardwareThreads >= 12 ? 3 : (hardwareThreads >= 4 ? 2 : 1));
    autoChunkWorkers = std::max<std::size_t>(1, hardwareThreads / autoCpuThreads);
    autoMaxJobs = std::min<std::size_t>(std::max<std::size_t>(1, autoChunkWorkers * 2), 24);
    autoReplicas = std::min<std::size_t>(autoMaxJobs, std::max<std::size_t>(1, hardwareThreads / std::max<std::size_t>(1, autoCpuThreads)));
    autoQueueSize = std::max<std::size_t>(64, autoMaxJobs * 4);
  }

  autoReplicas = memoryAwareReplicaCap(memoryBytes, autoReplicas);

  if (!runConfig.cpuThreadsExplicit && !runConfig.cpuThreads) {
    runConfig.cpuThreads = static_cast<int>(autoCpuThreads);
  }

  if (runConfig.maxConcurrentJobs == 0) {
    runConfig.maxConcurrentJobs = autoMaxJobs;
  }

  if (runConfig.chunkWorkers == 0) {
    runConfig.chunkWorkers = autoChunkWorkers;
  }

  if (runConfig.maxModelReplicas == 0) {
    runConfig.maxModelReplicas = autoReplicas;
  }

  if (runConfig.queueSize == 0) {
    runConfig.queueSize = std::max<std::size_t>(autoQueueSize, runConfig.maxConcurrentJobs);
  }

  if (!runConfig.maxTempBytesExplicit) {
    runConfig.maxTempBytes = autoTempBudgetBytes(memoryBytes);
  }

  // Prevent accidental oversubscription when the user overrides only part of the policy.
  const std::size_t cpuThreads = static_cast<std::size_t>(runConfig.cpuThreads.value_or(static_cast<int>(autoCpuThreads)));
  const std::size_t safeWorkersByCpu = std::max<std::size_t>(1, hardwareThreads / std::max<std::size_t>(1, cpuThreads));
  runConfig.chunkWorkers = clampSize(runConfig.chunkWorkers, 1, safeWorkersByCpu);
  runConfig.maxConcurrentJobs = std::max<std::size_t>(1, runConfig.maxConcurrentJobs);
  runConfig.maxModelReplicas = std::max<std::size_t>(1, std::min(runConfig.maxModelReplicas, runConfig.maxConcurrentJobs));
  runConfig.queueSize = std::max(runConfig.queueSize, runConfig.maxConcurrentJobs);
}

} // namespace piper_app
