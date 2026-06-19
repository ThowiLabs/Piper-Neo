#include "synthesis_input.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace piper_app {

std::unique_ptr<std::istream> openDirectInput(const RunConfig &runConfig,
                                              std::istream *&inputStream) {
  inputStream = &std::cin;

  if (runConfig.inputText) {
    auto textInput = std::make_unique<std::istringstream>(*runConfig.inputText);
    inputStream = textInput.get();
    return textInput;
  }

  if (runConfig.inputFilePath) {
    auto fileInput = std::make_unique<std::ifstream>(runConfig.inputFilePath->string(),
                                                     std::ios::binary);
    if (!fileInput->good()) {
      throw std::runtime_error("Input text file doesn't exist");
    }
    inputStream = fileInput.get();
    return fileInput;
  }

  return nullptr;
}

bool shouldUseDirectStreamMode(const RunConfig &runConfig) {
  return !runConfig.jsonInput &&
         (runConfig.inputText || runConfig.inputFilePath ||
          (runConfig.outputType == OUTPUT_FILE));
}

} // namespace piper_app
