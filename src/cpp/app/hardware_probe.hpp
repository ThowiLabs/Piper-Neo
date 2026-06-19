#ifndef PIPER_APP_HARDWARE_PROBE_HPP_
#define PIPER_APP_HARDWARE_PROBE_HPP_

#include <cstdint>

namespace piper_app {

unsigned int detectAvailableHardwareThreads();
uint64_t detectAvailableMemoryBytes();

} // namespace piper_app

#endif // PIPER_APP_HARDWARE_PROBE_HPP_
