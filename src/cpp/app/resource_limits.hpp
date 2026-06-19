#ifndef PIPER_APP_RESOURCE_LIMITS_HPP_
#define PIPER_APP_RESOURCE_LIMITS_HPP_

#include <cstddef>
#include <cstdint>

namespace piper_app {

std::size_t autoTempBudgetBytes(uint64_t memoryBytes);
std::size_t memoryAwareReplicaCap(uint64_t memoryBytes, std::size_t requestedCap);

} // namespace piper_app

#endif // PIPER_APP_RESOURCE_LIMITS_HPP_
