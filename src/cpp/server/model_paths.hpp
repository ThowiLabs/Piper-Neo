#ifndef PIPER_SERVER_MODEL_PATHS_H_
#define PIPER_SERVER_MODEL_PATHS_H_

#include <filesystem>
#include <string>

namespace piper_server {

std::string modelKey(const std::filesystem::path &modelPath);

} // namespace piper_server

#endif // PIPER_SERVER_MODEL_PATHS_H_
