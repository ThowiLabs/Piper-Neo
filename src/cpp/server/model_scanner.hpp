#ifndef PIPER_SERVER_MODEL_SCANNER_H_
#define PIPER_SERVER_MODEL_SCANNER_H_

#include <filesystem>
#include <optional>
#include <vector>

#include "../server.hpp"

namespace piper_server {

std::vector<ModelInfo> scanModels(const std::filesystem::path &modelsDir);
std::optional<ModelInfo> findFirstUsableModel(const std::filesystem::path &modelsDir);

} // namespace piper_server

#endif // PIPER_SERVER_MODEL_SCANNER_H_
