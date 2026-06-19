#include "core/sentence_splitter.hpp"

#include <cstddef>
#include <string>
#include <vector>

#include "core/sentence/boundary_detector.hpp"

namespace piper::core {

std::vector<ExplicitSentenceChunk> splitTextIntoExplicitSentenceChunks(const std::string &text) {
  std::vector<ExplicitSentenceChunk> chunks;
  std::size_t offset = 0;

  while (offset < text.size()) {
    std::size_t splitAt = std::string::npos;
    for (std::size_t i = offset; i < text.size(); ++i) {
      if (sentence::isUtf8ContinuationByte(text[i])) {
        continue;
      }

      if (sentence::isExplicitSentenceBoundary(text, i, splitAt)) {
        break;
      }
    }

    if (splitAt == std::string::npos) {
      auto tail = text.substr(offset);
      if (!tail.empty()) {
        chunks.push_back({tail, false});
      }
      break;
    }

    auto sentenceText = text.substr(offset, splitAt - offset);
    if (!sentenceText.empty()) {
      chunks.push_back({sentenceText, true});
    }

    offset = splitAt;
    while ((offset < text.size()) && sentence::isAsciiWhitespace(text[offset])) {
      ++offset;
    }
  }

  if (!chunks.empty()) {
    chunks.back().addSilenceAfter = false;
  }

  return chunks;
}

} // namespace piper::core
