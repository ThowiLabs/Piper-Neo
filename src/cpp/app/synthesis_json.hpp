#ifndef PIPER_APP_SYNTHESIS_JSON_HPP_
#define PIPER_APP_SYNTHESIS_JSON_HPP_

#include <filesystem>
#include <optional>

#include "run_config.hpp"
#include "../json.hpp"

namespace piper_app {

void applyJsonLineOverrides(const nlohmann::json &lineRoot, piper::Voice &voice,
                            OutputType &outputType,
                            std::optional<std::filesystem::path> &maybeOutputPath);

} // namespace piper_app

#endif // PIPER_APP_SYNTHESIS_JSON_HPP_
