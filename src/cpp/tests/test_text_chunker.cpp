#include "core/text_chunker.hpp"

#include <cassert>
#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool isUtf8Continuation(unsigned char value) {
  return (value & 0xC0) == 0x80;
}

bool hasSafeChunkEdges(const std::vector<std::string> &chunks) {
  for (const auto &chunk : chunks) {
    if (chunk.empty()) {
      return false;
    }
    if (isUtf8Continuation(static_cast<unsigned char>(chunk.front()))) {
      return false;
    }
  }
  return true;
}

std::string joinChunks(const std::vector<std::string> &chunks) {
  std::string joined;
  for (const auto &chunk : chunks) {
    joined += chunk;
  }
  return joined;
}

std::string withoutAsciiWhitespace(std::string value) {
  std::string compact;
  for (char ch : value) {
    if ((ch != ' ') && (ch != '\n') && (ch != '\r') && (ch != '\t') &&
        (ch != '\v') && (ch != '\f')) {
      compact.push_back(ch);
    }
  }
  return compact;
}

void testNoSplitWhenDisabled() {
  auto chunks = piper::splitTextIntoChunks("Texto corto", 0);
  assert(chunks.size() == 1);
  assert(chunks.front() == "Texto corto");
}

void testSentenceBoundaryPreferred() {
  const std::string text = "Primera oracion. Segunda oracion completa.";
  auto chunks = piper::splitTextIntoChunks(text, 22);
  assert(chunks.size() >= 2);
  assert(chunks.front() == "Primera oracion.");
  assert(withoutAsciiWhitespace(joinChunks(chunks)) == withoutAsciiWhitespace(text));
}

void testSpanishQuestionStaysTogether() {
  const std::string text = "¿Cuanto cuesta este producto hoy? Esta bien.";
  auto chunks = piper::splitTextIntoChunks(text, 18);
  assert(chunks.size() >= 2);
  assert(chunks.front() == "¿Cuanto cuesta este producto hoy?");
  assert(withoutAsciiWhitespace(joinChunks(chunks)) == withoutAsciiWhitespace(text));
}

void testUtf8BoundariesAreSafe() {
  const std::string text = "áéíóú ñandú rápido. Otro texto con acentos.";
  auto chunks = piper::splitTextIntoChunks(text, 7);
  assert(chunks.size() > 1);
  assert(hasSafeChunkEdges(chunks));
  assert(withoutAsciiWhitespace(joinChunks(chunks)) == withoutAsciiWhitespace(text));
}

void testLongWordUsesHardLimit() {
  const std::string text = "supercalifragilisticoespialidoso";
  auto chunks = piper::splitTextIntoChunks(text, 8);
  assert(chunks.size() > 1);
  for (const auto &chunk : chunks) {
    assert(chunk.size() <= 16);
  }
  assert(withoutAsciiWhitespace(joinChunks(chunks)) == withoutAsciiWhitespace(text));
}

} // namespace

int main() {
  testNoSplitWhenDisabled();
  testSentenceBoundaryPreferred();
  testSpanishQuestionStaysTogether();
  testUtf8BoundariesAreSafe();
  testLongWordUsesHardLimit();
  std::cout << "TEST_OK" << std::endl;
  return 0;
}
