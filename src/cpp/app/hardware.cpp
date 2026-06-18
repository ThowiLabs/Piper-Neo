#include "hardware.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <thread>

#ifdef __linux__
#include <sched.h>
#include <unistd.h>
#endif

#include <spdlog/spdlog.h>

namespace piper_app {

using namespace std;

size_t clampSize(size_t value, size_t minValue, size_t maxValue) {
  return std::max(minValue, std::min(value, maxValue));
}

uint64_t clampU64(uint64_t value, uint64_t minValue, uint64_t maxValue) {
  return std::max(minValue, std::min(value, maxValue));
}

optional<string> readTextFileQuiet(const filesystem::path &path) {
  ifstream file(path);
  if (!file.good()) {
    return nullopt;
  }

  string value;
  getline(file, value);
  return value;
}

optional<uint64_t> parseUnsignedQuiet(const string &value) {
  try {
    size_t pos = 0;
    auto parsed = stoull(value, &pos);
    if (pos == 0) {
      return nullopt;
    }
    return parsed;
  } catch (...) {
    return nullopt;
  }
}

optional<unsigned int> detectCgroupCpuQuota() {
#ifdef __linux__
  // cgroup v2: "max 100000" means unlimited; "200000 100000" means 2 CPUs.
  if (auto cpuMax = readTextFileQuiet("/sys/fs/cgroup/cpu.max")) {
    istringstream stream(*cpuMax);
    string quotaText;
    string periodText;
    stream >> quotaText >> periodText;
    if (!quotaText.empty() && quotaText != "max" && !periodText.empty()) {
      auto quota = parseUnsignedQuiet(quotaText);
      auto period = parseUnsignedQuiet(periodText);
      if (quota && period && *period > 0) {
        return static_cast<unsigned int>(std::max<uint64_t>(1, (*quota + *period - 1) / *period));
      }
    }
  }

  // cgroup v1 fallback.
  auto quotaText = readTextFileQuiet("/sys/fs/cgroup/cpu/cpu.cfs_quota_us");
  auto periodText = readTextFileQuiet("/sys/fs/cgroup/cpu/cpu.cfs_period_us");
  if (quotaText && periodText) {
    try {
      long long quota = stoll(*quotaText);
      long long period = stoll(*periodText);
      if (quota > 0 && period > 0) {
        return static_cast<unsigned int>(std::max<long long>(1, (quota + period - 1) / period));
      }
    } catch (...) {
    }
  }
#endif

  return nullopt;
}

optional<unsigned int> detectAffinityCpuCount() {
#ifdef __linux__
  cpu_set_t cpuSet;
  CPU_ZERO(&cpuSet);
  if (sched_getaffinity(0, sizeof(cpuSet), &cpuSet) == 0) {
    const int count = CPU_COUNT(&cpuSet);
    if (count > 0) {
      return static_cast<unsigned int>(count);
    }
  }
#endif

  return nullopt;
}

unsigned int detectAvailableHardwareThreads() {
  unsigned int detected = std::thread::hardware_concurrency();
  if (detected == 0) {
    detected = 2;
  }

  if (auto affinity = detectAffinityCpuCount()) {
    detected = std::min(detected, *affinity);
  }

  if (auto cgroupQuota = detectCgroupCpuQuota()) {
    detected = std::min(detected, *cgroupQuota);
  }

  return std::max(1u, detected);
}

optional<uint64_t> detectCgroupMemoryLimitBytes() {
#ifdef __linux__
  constexpr uint64_t unreasonableLimit = 1ULL << 60;

  // cgroup v2.
  if (auto memoryMax = readTextFileQuiet("/sys/fs/cgroup/memory.max")) {
    if (*memoryMax != "max") {
      if (auto parsed = parseUnsignedQuiet(*memoryMax)) {
        if (*parsed > 0 && *parsed < unreasonableLimit) {
          return parsed;
        }
      }
    }
  }

  // cgroup v1 fallback.
  if (auto memoryLimit = readTextFileQuiet("/sys/fs/cgroup/memory/memory.limit_in_bytes")) {
    if (auto parsed = parseUnsignedQuiet(*memoryLimit)) {
      if (*parsed > 0 && *parsed < unreasonableLimit) {
        return parsed;
      }
    }
  }
#endif

  return nullopt;
}

uint64_t detectAvailableMemoryBytes() {
  uint64_t detected = 0;

#ifdef __linux__
  const long pages = sysconf(_SC_PHYS_PAGES);
  const long pageSize = sysconf(_SC_PAGE_SIZE);
  if (pages > 0 && pageSize > 0) {
    detected = static_cast<uint64_t>(pages) * static_cast<uint64_t>(pageSize);
  }
#endif

  if (auto cgroupLimit = detectCgroupMemoryLimitBytes()) {
    detected = detected == 0 ? *cgroupLimit : std::min(detected, *cgroupLimit);
  }

  return detected;
}

size_t autoTempBudgetBytes(uint64_t memoryBytes) {
  constexpr uint64_t MiB = 1024ULL * 1024ULL;
  constexpr uint64_t GiB = 1024ULL * MiB;

  if (memoryBytes == 0) {
    return static_cast<size_t>(4ULL * GiB);
  }

  if (memoryBytes < 1024ULL * MiB) {
    return static_cast<size_t>(clampU64(memoryBytes / 8, 128ULL * MiB, 256ULL * MiB));
  }

  if (memoryBytes < 4ULL * GiB) {
    return static_cast<size_t>(clampU64(memoryBytes / 6, 256ULL * MiB, 768ULL * MiB));
  }

  return static_cast<size_t>(clampU64(memoryBytes / 4, 1ULL * GiB, 16ULL * GiB));
}

size_t memoryAwareReplicaCap(uint64_t memoryBytes, size_t requestedCap) {
  if (memoryBytes == 0) {
    return requestedCap;
  }

  constexpr uint64_t GiB = 1024ULL * 1024ULL * 1024ULL;
  size_t memoryCap = 1;
  if (memoryBytes >= 32ULL * GiB) {
    memoryCap = 8;
  } else if (memoryBytes >= 16ULL * GiB) {
    memoryCap = 6;
  } else if (memoryBytes >= 8ULL * GiB) {
    memoryCap = 4;
  } else if (memoryBytes >= 4ULL * GiB) {
    memoryCap = 3;
  } else if (memoryBytes >= 2ULL * GiB) {
    memoryCap = 2;
  }

  return std::max<size_t>(1, std::min(requestedCap, memoryCap));
}

void applyAutoServerResourceConfig(RunConfig &runConfig) {
  const unsigned int hardwareThreads = detectAvailableHardwareThreads();
  const uint64_t memoryBytes = detectAvailableMemoryBytes();
  runConfig.detectedHardwareThreads = hardwareThreads;
  runConfig.detectedMemoryBytes = memoryBytes;

  size_t autoCpuThreads = 1;
  size_t autoMaxJobs = 1;
  size_t autoChunkWorkers = 1;
  size_t autoReplicas = 1;
  size_t autoQueueSize = 16;

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
    autoChunkWorkers = std::max<size_t>(1, hardwareThreads / autoCpuThreads);
    autoMaxJobs = std::min<size_t>(std::max<size_t>(2, autoChunkWorkers * 2), 16);
    autoReplicas = std::min<size_t>(autoMaxJobs, hardwareThreads >= 16 ? 5 : (hardwareThreads >= 8 ? 4 : 2));
    autoQueueSize = 64;
  } else if (runConfig.cpuProfile == "max") {
    autoCpuThreads = hardwareThreads >= 32 ? 4 : (hardwareThreads >= 12 ? 3 : (hardwareThreads >= 4 ? 2 : 1));
    autoChunkWorkers = std::max<size_t>(1, hardwareThreads / autoCpuThreads);
    autoMaxJobs = std::min<size_t>(std::max<size_t>(2, autoChunkWorkers * 2), 32);
    autoReplicas = std::min<size_t>(autoMaxJobs, std::max<size_t>(2, hardwareThreads / 2));
    autoQueueSize = 96;
  } else {
    // auto: default profile. Saturates the CPU budget exposed by the OS/container
    // while keeping per-worker ONNX threads and model replicas bounded by RAM.
    autoCpuThreads = hardwareThreads >= 32 ? 4 : (hardwareThreads >= 12 ? 3 : (hardwareThreads >= 4 ? 2 : 1));
    autoChunkWorkers = std::max<size_t>(1, hardwareThreads / autoCpuThreads);
    autoMaxJobs = std::min<size_t>(std::max<size_t>(1, autoChunkWorkers * 2), 24);
    autoReplicas = std::min<size_t>(autoMaxJobs, std::max<size_t>(1, hardwareThreads / std::max<size_t>(1, autoCpuThreads)));
    autoQueueSize = std::max<size_t>(64, autoMaxJobs * 4);
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
    runConfig.queueSize = std::max<size_t>(autoQueueSize, runConfig.maxConcurrentJobs);
  }

  if (!runConfig.maxTempBytesExplicit) {
    runConfig.maxTempBytes = autoTempBudgetBytes(memoryBytes);
  }

  // Prevent accidental oversubscription when the user overrides only part of the policy.
  const size_t cpuThreads = static_cast<size_t>(runConfig.cpuThreads.value_or(autoCpuThreads));
  const size_t safeWorkersByCpu = std::max<size_t>(1, hardwareThreads / std::max<size_t>(1, cpuThreads));
  runConfig.chunkWorkers = clampSize(runConfig.chunkWorkers, 1, safeWorkersByCpu);
  runConfig.maxConcurrentJobs = std::max<size_t>(1, runConfig.maxConcurrentJobs);
  runConfig.maxModelReplicas = std::max<size_t>(1, std::min(runConfig.maxModelReplicas, runConfig.maxConcurrentJobs));
  runConfig.queueSize = std::max(runConfig.queueSize, runConfig.maxConcurrentJobs);

  spdlog::info("Server resource policy: profile={} detected_threads={} detected_memory_mb={} cpu_threads={} chunk_workers={} max_jobs={} max_model_replicas={} queue_size={} max_temp_bytes={}",
               runConfig.cpuProfile, hardwareThreads,
               memoryBytes == 0 ? 0 : memoryBytes / (1024ULL * 1024ULL),
               runConfig.cpuThreads.value_or(0), runConfig.chunkWorkers,
               runConfig.maxConcurrentJobs, runConfig.maxModelReplicas,
               runConfig.queueSize, runConfig.maxTempBytes);
}


} // namespace piper_app
