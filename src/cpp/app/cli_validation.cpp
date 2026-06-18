#include "cli_validation.hpp"

#include "hardware.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>

#include "../neo_model.hpp"
#include "../server.hpp"

namespace piper_app {

using namespace std;

void finalizeRunConfig(
    RunConfig &runConfig,
    const std::optional<std::filesystem::path> &modelConfigPathInput) {
  auto modelConfigPath = modelConfigPathInput;


  if (runConfig.inputText && runConfig.inputFilePath) {
    throw runtime_error("Use either --text or --input-file, not both");
  }

  if (runConfig.inputFilePath) {
    ifstream inputFile(runConfig.inputFilePath->c_str(), ios::binary);
    if (!inputFile.good()) {
      throw runtime_error("Input text file doesn't exist");
    }
  }

  if (runConfig.serverMode) {
    // Be forgiving: many users naturally pass a models folder with --model.
    // In server mode, a directory supplied to --model is treated as --models.
    if (!runConfig.modelPath.empty() && filesystem::is_directory(runConfig.modelPath)) {
      runConfig.modelsDir = runConfig.modelPath;
      runConfig.modelPath.clear();
      modelConfigPath.reset();
    }

    filesystem::create_directories(runConfig.modelsDir);

    if (runConfig.modelPath.empty()) {
      auto maybeModel = piper_server::findFirstUsableModel(runConfig.modelsDir);
      if (maybeModel) {
        runConfig.modelPath = maybeModel->modelPath;
        if (!modelConfigPath && !maybeModel->isNeo) {
          modelConfigPath = maybeModel->configPath;
        }
      } else {
        throw runtime_error(
            "No usable .onnx/.neo model found in models directory: " +
            runConfig.modelsDir.string() +
            ". Copy a .neo file or a .onnx file with its .onnx.json config, "
            "or pass --models DIR / --model FILE.");
      }
    } else if (!filesystem::exists(runConfig.modelPath)) {
      auto modelInModelsDir = runConfig.modelsDir / runConfig.modelPath;
      if (filesystem::exists(modelInModelsDir)) {
        runConfig.modelPath = modelInModelsDir;
      }
    }
  }

  if (runConfig.serverMode) {
    applyAutoServerResourceConfig(runConfig);
  }

  if (runConfig.modelPath.empty()) {
    throw runtime_error("Model file is required. Use --model FILE");
  }

  // Verify model file exists
  ifstream modelFile(runConfig.modelPath.c_str(), ios::binary);
  if (!modelFile.good()) {
    throw runtime_error("Model file doesn't exist");
  }

  if (piper_neo::isNeoFile(runConfig.modelPath)) {
    runConfig.modelConfigPath.clear();
  } else if (!modelConfigPath) {
    runConfig.modelConfigPath =
        filesystem::path(runConfig.modelPath.string() + ".json");
  } else {
    runConfig.modelConfigPath = modelConfigPath.value();
  }

  // Verify model config exists for the classic .onnx format. .neo embeds it.
  if (!piper_neo::isNeoFile(runConfig.modelPath)) {
    ifstream modelConfigFile(runConfig.modelConfigPath.c_str());
    if (!modelConfigFile.good()) {
      throw runtime_error("Model config doesn't exist");
    }
  }

  if (runConfig.exportNeoPath && piper_neo::isNeoFile(runConfig.modelPath)) {
    throw runtime_error("--export-neo expects a classic .onnx model plus .onnx.json config");
  }
}

} // namespace piper_app
