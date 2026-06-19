#include "protected_segments.hpp"

#include <cstddef>

namespace piper::textnorm {

std::string makeProtectedMarker(std::size_t index) {
  return std::string("\x1F") + std::to_string(index) + "\x1F";
}

void appendProtectedSegment(ProtectedTextBuffer &buffer,
                            const std::string &speechText) {
  const auto index = buffer.segments.size();
  buffer.text += makeProtectedMarker(index);
  buffer.segments.push_back(speechText);
}

std::string restoreProtectedSegments(const std::string &text,
                                     const std::vector<std::string> &segments) {
  std::string out;
  out.reserve(text.size());

  for (std::size_t i = 0; i < text.size();) {
    if (text[i] == '\x1F') {
      const auto end = text.find('\x1F', i + 1);
      if (end != std::string::npos) {
        const auto indexText = text.substr(i + 1, end - i - 1);
        try {
          const auto index = static_cast<std::size_t>(std::stoul(indexText));
          if (index < segments.size()) {
            out += segments[index];
            i = end + 1;
            continue;
          }
        } catch (...) {
        }
      }
    }

    out.push_back(text[i]);
    ++i;
  }

  return out;
}

} // namespace piper::textnorm
