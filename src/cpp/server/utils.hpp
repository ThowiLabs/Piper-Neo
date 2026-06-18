#ifndef PIPER_SERVER_UTILS_H_
#define PIPER_SERVER_UTILS_H_

#include <filesystem>
#include <optional>
#include <string>

#include "types.hpp"

namespace piper_server {

std::string nowIso8601();
std::string lowerCopy(std::string value);
std::string trimCopy(const std::string &value);
bool isSafeFileName(const std::string &fileName);
std::string makeOutputFileName();
json loadJsonFile(const std::filesystem::path &path);
std::optional<json> tryLoadJsonFile(const std::filesystem::path &path,
                                    std::string &error);
std::string decodeBase64(const std::string &encoded);
std::pair<std::string, std::string> parseDataImage(const std::string &dataUri);

} // namespace piper_server

#endif // PIPER_SERVER_UTILS_H_
