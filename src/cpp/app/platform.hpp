#ifndef PIPER_APP_PLATFORM_HPP_
#define PIPER_APP_PLATFORM_HPP_

#include <filesystem>

namespace piper_app {

void configureConsoleUtf8();
std::filesystem::path resolveExecutablePath(const char *argv0);

} // namespace piper_app

#endif // PIPER_APP_PLATFORM_HPP_
