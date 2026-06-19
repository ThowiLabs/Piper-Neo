#include "resource_limits.hpp"

#include <algorithm>
#include <cstdint>
#include <cstddef>

namespace piper_app {
namespace {

uint64_t clampU64(uint64_t value, uint64_t minValue, uint64_t maxValue) {
  return std::max(minValue, std::min(value, maxValue));
}

} // namespace

std::size_t autoTempBudgetBytes(uint64_t memoryBytes) {
  constexpr uint64_t MiB = 1024ULL * 1024ULL;
  constexpr uint64_t GiB = 1024ULL * MiB;

  if (memoryBytes == 0) {
    return static_cast<std::size_t>(4ULL * GiB);
  }

  if (memoryBytes < 1024ULL * MiB) {
    return static_cast<std::size_t>(clampU64(memoryBytes / 8, 128ULL * MiB, 256ULL * MiB));
  }

  if (memoryBytes < 4ULL * GiB) {
    return static_cast<std::size_t>(clampU64(memoryBytes / 6, 256ULL * MiB, 768ULL * MiB));
  }

  return static_cast<std::size_t>(clampU64(memoryBytes / 4, 1ULL * GiB, 16ULL * GiB));
}

std::size_t memoryAwareReplicaCap(uint64_t memoryBytes, std::size_t requestedCap) {
  if (memoryBytes == 0) {
    return requestedCap;
  }

  constexpr uint64_t GiB = 1024ULL * 1024ULL * 1024ULL;
  std::size_t memoryCap = 1;
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

  return std::max<std::size_t>(1, std::min(requestedCap, memoryCap));
}

} // namespace piper_app
