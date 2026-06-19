#include "core/sentence_splitter.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string &message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << std::endl;
    std::exit(1);
  }
}

std::vector<std::string> texts(const std::vector<piper::core::ExplicitSentenceChunk> &chunks) {
  std::vector<std::string> out;
  for (const auto &chunk : chunks) {
    out.push_back(chunk.text);
  }
  return out;
}

void requireTexts(const std::string &input, const std::vector<std::string> &expected) {
  const auto chunks = piper::core::splitTextIntoExplicitSentenceChunks(input);
  const auto actual = texts(chunks);
  require(actual == expected, "unexpected sentence split for: " + input);
  if (!chunks.empty()) {
    require(!chunks.back().addSilenceAfter, "last chunk must not request trailing silence");
  }
}

} // namespace

int main() {
  requireTexts("Hola. Mundo.", {"Hola.", "Mundo."});
  requireTexts("El valor es 3.14. Siguiente.", {"El valor es 3.14.", "Siguiente."});
  requireTexts("Dr. López llegó. Después habló.", {"Dr. López llegó.", "Después habló."});
  requireTexts("¿Todo bien? Sí.", {"¿Todo bien?", "Sí."});
  requireTexts("Dijo: \"hola.\" Luego salió.", {"Dijo: \"hola.\"", "Luego salió."});
  requireTexts("Uno… Dos.", {"Uno…", "Dos."});
  requireTexts("Primera línea\n\nSegunda línea", {"Primera línea\n\n", "Segunda línea"});
  requireTexts("Sin cortes intermedios", {"Sin cortes intermedios"});

  const auto chunks = piper::core::splitTextIntoExplicitSentenceChunks("A. B. C.");
  require(chunks.size() == 3, "expected 3 chunks");
  require(chunks[0].addSilenceAfter, "first chunk should request silence");
  require(chunks[1].addSilenceAfter, "second chunk should request silence");
  require(!chunks[2].addSilenceAfter, "last chunk should not request silence");

  std::cout << "TEST_OK" << std::endl;
  return 0;
}
