#include "core/text_chunker.hpp"

#include "core/text/chunk_rules.hpp"
#include "core/text/utf8_utils.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace piper {

std::vector<std::string> splitTextIntoChunks(const std::string &text,
                                             std::size_t maxChunkBytes) {
  if ((maxChunkBytes == 0) || (text.size() <= maxChunkBytes)) {
    return {text};
  }

  std::vector<std::string> chunks;
  std::size_t offset = 0;

  while (offset < text.size()) {
    text::trimLeadingAsciiWhitespace(text, offset);
    if (offset >= text.size()) {
      break;
    }

    std::size_t splitAt = text::chooseSmartSplitPoint(text, offset, maxChunkBytes);
    splitAt = text::clampToUtf8Boundary(text, splitAt);

    if (splitAt <= offset) {
      splitAt = text::clampToUtf8Boundary(
          text, std::min(offset + maxChunkBytes, text.size()));
    }

    chunks.emplace_back(text.substr(offset, splitAt - offset));
    offset = splitAt;
  }

  return chunks;
}

} // namespace piper
