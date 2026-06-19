#include "app/resource_limits.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>

namespace {

constexpr uint64_t MiB = 1024ULL * 1024ULL;
constexpr uint64_t GiB = 1024ULL * MiB;

void testTempBudget() {
  assert(piper_app::autoTempBudgetBytes(0) == 4ULL * GiB);
  assert(piper_app::autoTempBudgetBytes(512ULL * MiB) >= 128ULL * MiB);
  assert(piper_app::autoTempBudgetBytes(512ULL * MiB) <= 256ULL * MiB);
  assert(piper_app::autoTempBudgetBytes(8ULL * GiB) >= 1ULL * GiB);
}

void testMemoryReplicaCap() {
  assert(piper_app::memoryAwareReplicaCap(0, 9) == 9);
  assert(piper_app::memoryAwareReplicaCap(1ULL * GiB, 9) == 1);
  assert(piper_app::memoryAwareReplicaCap(8ULL * GiB, 9) == 4);
}

} // namespace

int main() {
  testTempBudget();
  testMemoryReplicaCap();
  std::cout << "TEST_OK" << std::endl;
  return 0;
}
