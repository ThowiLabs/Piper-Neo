#include "server/model_registry.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace {

void require(bool condition, const std::string &message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << std::endl;
    std::exit(1);
  }
}

void writeText(const fs::path &path, const std::string &text) {
  std::ofstream out(path, std::ios::binary);
  out << text;
}

piper_server::ServerOptions makeOptions(const fs::path &dir) {
  piper_server::ServerOptions options;
  options.modelsDir = dir;
  options.modelsRefreshSeconds = 1;
  options.activeModelPath = dir / "active.onnx";
  options.activeModelConfigPath = dir / "active.onnx.json";
  return options;
}

} // namespace

int main(int argc, char **argv) {
  require(argc >= 2, "temporary directory argument is required");
  const fs::path dir = fs::path(argv[1]);
  fs::remove_all(dir);
  fs::create_directories(dir);

  writeText(dir / "active.onnx", "fake-active");
  writeText(dir / "active.onnx.json", R"json({
    "modelcard": {"name": "Active Voice", "language": "es_MX"},
    "audio": {"sample_rate": 22050, "quality": "medium"},
    "espeak": {"voice": "es"},
    "inference": {"noise_scale": 0.667, "length_scale": 1.0},
    "num_speakers": 1,
    "phoneme_type": "espeak",
    "speaker_id_map": {"default": 0}
  })json");

  writeText(dir / "bravo.onnx", "fake-bravo");
  writeText(dir / "bravo.onnx.json", R"json({
    "modelcard": {"name": "Bravo", "language": "es"},
    "audio": {"sample_rate": 16000},
    "inference": {"noise_scale": 0.5}
  })json");

  writeText(dir / "charlie.onnx", "fake-charlie");
  writeText(dir / "delta.neo", "not-a-real-neo-package");
  writeText(dir / "ignore.txt", "ignore me");

  const auto scanned = piper_server::scanModels(dir);
  require(scanned.size() == 4, "scanModels should include .onnx and .neo only");
  require(scanned[0].name == "active.onnx", "models should be sorted by filename");
  require(scanned[1].name == "bravo.onnx", "bravo should be second");
  require(scanned[2].name == "charlie.onnx", "charlie should be third");
  require(scanned[3].name == "delta.neo", "neo package should be listed");
  require(scanned[1].hasConfig, "bravo should have config");
  require(!scanned[2].hasConfig, "charlie should not have config");
  require(scanned[3].isNeo && scanned[3].hasConfig, "neo packages should be marked as embedded config");

  const auto first = piper_server::findFirstUsableModel(dir);
  require(first && first->name == "active.onnx", "first usable model should be active.onnx");

  const auto options = makeOptions(dir);
  piper_server::ModelRegistry registry(options);
  const auto list = registry.list("technical");
  require(list.is_array() && list.size() == 4, "registry list should expose all models");
  require(list[1]["name"].get<std::string>() == "Bravo", "modelcard name should be exposed");
  require(list[1]["audio"]["sample_rate"].get<int>() == 16000, "audio metadata should be exposed");

  const auto foundByExtension = piper_server::findModelByName(options, "bravo.onnx", &registry);
  require(foundByExtension && foundByExtension->name == "bravo.onnx", "find by full filename failed");

  const auto foundByStem = piper_server::findModelByName(options, "bravo", &registry);
  require(foundByStem && foundByStem->name == "bravo.onnx", "find by stem should add .onnx");

  const auto fallbackActive = piper_server::findModelByName(options, "", &registry);
  require(fallbackActive && fallbackActive->name == "active.onnx", "empty model should resolve active model");

  bool rejectedUnsafeName = false;
  try {
    (void)piper_server::findModelByName(options, "../active.onnx", &registry);
  } catch (const std::runtime_error &e) {
    rejectedUnsafeName = std::string(e.what()) == "invalid_model";
  }
  require(rejectedUnsafeName, "unsafe model names must be rejected");

  fs::remove_all(dir);
  std::cout << "TEST_OK" << std::endl;
  return 0;
}
