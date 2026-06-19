#ifndef PIPER_APP_RESOURCE_POLICY_HPP_
#define PIPER_APP_RESOURCE_POLICY_HPP_

#include "run_config.hpp"

#include <cstdint>

namespace piper_app {
void applyServerResourcePolicy(RunConfig &runConfig, unsigned int hardwareThreads,
                               uint64_t memoryBytes);

} // namespace piper_app

#endif // PIPER_APP_RESOURCE_POLICY_HPP_
