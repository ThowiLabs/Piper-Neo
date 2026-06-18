#ifndef PIPER_NEO_FILE_UTILS_H_
#define PIPER_NEO_FILE_UTILS_H_

#include <filesystem>
#include <string>
#include <vector>

namespace piper_neo::detail {

std::string lowerCopy(std::string value);
std::string fnv1aHex(const std::string &input);

std::vector<char> readFileBytes(const std::filesystem::path &path);
void writeFileBytes(const std::filesystem::path &path, const std::vector<char> &bytes);

} // namespace piper_neo::detail

#endif // PIPER_NEO_FILE_UTILS_H_
