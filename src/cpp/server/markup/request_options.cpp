#include "server/markup/request_options.hpp"

#include <stdexcept>

namespace piper_server {

std::optional<float> requestFloatOption(const json &input, const std::vector<std::string> &keys,
                                        const std::string &field, float minValue,
                                        float maxValue) {
  for (const auto &key : keys) {
    if (!input.contains(key)) {
      continue;
    }
    if (!input[key].is_number()) {
      throw std::runtime_error("invalid_request_" + field);
    }
    const auto value = input[key].get<float>();
    if (value < minValue || value > maxValue) {
      throw std::runtime_error("invalid_request_" + field);
    }
    return value;
  }
  return std::nullopt;
}

} // namespace piper_server
