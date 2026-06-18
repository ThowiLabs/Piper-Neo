#ifndef PIPER_SERVER_MARKUP_REQUEST_OPTIONS_H_
#define PIPER_SERVER_MARKUP_REQUEST_OPTIONS_H_

#include <optional>
#include <string>
#include <vector>

#include "../types.hpp"

namespace piper_server {

std::optional<float> requestFloatOption(const json &input, const std::vector<std::string> &keys,
                                        const std::string &field, float minValue,
                                        float maxValue);

} // namespace piper_server

#endif // PIPER_SERVER_MARKUP_REQUEST_OPTIONS_H_
