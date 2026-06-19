#ifndef PIPER_SERVER_MEDIA_DATA_IMAGE_HPP_
#define PIPER_SERVER_MEDIA_DATA_IMAGE_HPP_

#include <string>
#include <utility>

namespace piper_server {

std::pair<std::string, std::string> parseDataImage(const std::string &dataUri);

} // namespace piper_server

#endif // PIPER_SERVER_MEDIA_DATA_IMAGE_HPP_
