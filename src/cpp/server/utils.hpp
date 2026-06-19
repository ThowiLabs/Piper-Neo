#ifndef PIPER_SERVER_UTILS_H_
#define PIPER_SERVER_UTILS_H_

#include <filesystem>
#include <optional>
#include <string>

#include "../json.hpp"

namespace piper_server {

using json = nlohmann::json;

std::string nowIso8601();
std::string lowerCopy(std::string value);
std::string trimCopy(const std::string &value);
bool isSafeFileName(const std::string &fileName);
std::string makeOutputFileName();
json loadJsonFile(const std::filesystem::path &path);
std::optional<json> tryLoadJsonFile(const std::filesystem::path &path,
                                    std::string &error);

} // namespace piper_server

#endif // PIPER_SERVER_UTILS_H_
