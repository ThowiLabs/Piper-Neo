#ifndef PIPER_NEO_IMAGE_PAYLOAD_H_
#define PIPER_NEO_IMAGE_PAYLOAD_H_

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace piper_neo::detail {

std::string contentTypeForImagePath(const std::filesystem::path &path);
std::pair<std::string, std::vector<char>> decodeDataImage(const std::string &dataUri);

} // namespace piper_neo::detail

#endif // PIPER_NEO_IMAGE_PAYLOAD_H_
