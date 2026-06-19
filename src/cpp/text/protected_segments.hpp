#ifndef PIPER_TEXT_PROTECTED_SEGMENTS_HPP_
#define PIPER_TEXT_PROTECTED_SEGMENTS_HPP_

#include <string>
#include <vector>

namespace piper::textnorm {

struct ProtectedTextBuffer {
  std::string text;
  std::vector<std::string> segments;
};

std::string makeProtectedMarker(std::size_t index);
void appendProtectedSegment(ProtectedTextBuffer &buffer,
                            const std::string &speechText);
std::string restoreProtectedSegments(const std::string &text,
                                     const std::vector<std::string> &segments);

} // namespace piper::textnorm

#endif // PIPER_TEXT_PROTECTED_SEGMENTS_HPP_
