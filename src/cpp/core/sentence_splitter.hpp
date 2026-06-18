#ifndef PIPER_CORE_SENTENCE_SPLITTER_HPP_
#define PIPER_CORE_SENTENCE_SPLITTER_HPP_

#include <string>
#include <vector>

namespace piper::core {

struct ExplicitSentenceChunk {
  std::string text;
  bool addSilenceAfter = false;
};

std::vector<ExplicitSentenceChunk> splitTextIntoExplicitSentenceChunks(
    const std::string &text);

} // namespace piper::core

#endif // PIPER_CORE_SENTENCE_SPLITTER_HPP_
