#ifndef PIPER_APP_ENV_HPP_
#define PIPER_APP_ENV_HPP_

#include <optional>
#include <string>

namespace piper_app {

std::optional<std::string> resolveApiToken(const std::optional<std::string> &explicitToken);

} // namespace piper_app

#endif // PIPER_APP_ENV_HPP_
