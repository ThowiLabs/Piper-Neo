#include "hardware_probe.hpp"

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

namespace piper_app {
namespace {

std::optional<std::string> readTextFileQuiet(const std::filesystem::path &path) {
  std::ifstream file(path);
  if (!file.good()) {
    return std::nullopt;
  }

  std::string value;
  std::getline(file, value);
  return value;
}

std::optional<uint64_t> parseUnsignedQuiet(const std::string &value) {
  try {
    size_t pos = 0;
    auto parsed = std::stoull(value, &pos);
    if (pos == 0) {
      return std::nullopt;
    }
    return parsed;
  } catch (...) {
    return std::nullopt;
  }
}

std::optional<unsigned int> detectCgroupCpuQuota() {
#ifdef __linux__
  // cgroup v2: "max 100000" means unlimited; "200000 100000" means 2 CPUs.
  if (auto cpuMax = readTextFileQuiet("/sys/fs/cgroup/cpu.max")) {
    std::istringstream stream(*cpuMax);
    std::string quotaText;
    std::string periodText;
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
      long long quota = std::stoll(*quotaText);
      long long period = std::stoll(*periodText);
      if (quota > 0 && period > 0) {
        return static_cast<unsigned int>(std::max<long long>(1, (quota + period - 1) / period));
      }
    } catch (...) {
    }
  }
#endif

  return std::nullopt;
}

std::optional<unsigned int> detectAffinityCpuCount() {
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

  return std::nullopt;
}

std::optional<uint64_t> detectCgroupMemoryLimitBytes() {
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

  return std::nullopt;
}

} // namespace

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

} // namespace piper_app
