#include "platform.hpp"

#include <cstdint>
#include <iterator>
#include <stdexcept>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <limits.h>
#endif

#ifdef __linux__
#include <unistd.h>
#endif

namespace piper_app {

void configureConsoleUtf8() {
#ifdef _WIN32
  SetConsoleOutputCP(CP_UTF8);
  SetConsoleCP(CP_UTF8);
#endif
}

std::filesystem::path resolveExecutablePath(const char *argv0) {
#ifdef _WIN32
  wchar_t moduleFileName[MAX_PATH] = {0};
  DWORD moduleFileNameSize = GetModuleFileNameW(
      nullptr, moduleFileName, static_cast<DWORD>(std::size(moduleFileName)));

  if (moduleFileNameSize == 0 ||
      moduleFileNameSize >= static_cast<DWORD>(std::size(moduleFileName))) {
    throw std::runtime_error("Unable to resolve Piper executable path on Windows");
  }

  return std::filesystem::path(moduleFileName);
#elif defined(__APPLE__)
  char moduleFileName[PATH_MAX] = {0};
  uint32_t moduleFileNameSize = std::size(moduleFileName);
  if (_NSGetExecutablePath(moduleFileName, &moduleFileNameSize) != 0) {
    throw std::runtime_error("Unable to resolve Piper executable path on macOS");
  }
  return std::filesystem::path(moduleFileName);
#elif defined(__linux__)
  return std::filesystem::canonical("/proc/self/exe");
#else
  return std::filesystem::absolute(argv0 == nullptr ? "piper" : argv0);
#endif
}

} // namespace piper_app
