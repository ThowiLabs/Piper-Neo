#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "neo/binary_io.hpp"
#include "neo/constants.hpp"
#include "neo_model.hpp"

namespace fs = std::filesystem;

namespace {

struct TestSection {
  std::string name;
  std::string contentType;
  std::vector<char> payload;
  std::uint64_t offset = 0;
};

std::vector<char> bytes(const std::string &value) {
  return std::vector<char>(value.begin(), value.end());
}

std::uint64_t directorySize(const std::vector<TestSection> &sections) {
  std::uint64_t size = 8 + 4 + 4;
  for (const auto &section : sections) {
    size += 4 + section.name.size();
    size += 4 + section.contentType.size();
    size += 4 + 8 + 8 + 8;
  }
  return size;
}

void writeFixturePackage(const fs::path &path) {
  std::vector<TestSection> sections{
      {"metadata.json", "application/json",
       bytes(R"({"piper_neo":{"format":"piper-neo","format_version":1},"audio":{"sample_rate":22050}})")},
      {"model.onnx", "application/onnx", bytes("fake-onnx-bytes")},
      {"image", "image/png", bytes("fake-png-bytes")},
  };

  auto offset = directorySize(sections);
  for (auto &section : sections) {
    section.offset = offset;
    offset += section.payload.size();
  }

  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary);
  if (!out.good()) {
    throw std::runtime_error("could not open fixture");
  }

  out.write(piper_neo::detail::NEO_MAGIC.data(),
            static_cast<std::streamsize>(piper_neo::detail::NEO_MAGIC.size()));
  piper_neo::detail::writeU32(out, piper_neo::detail::NEO_FORMAT_VERSION);
  piper_neo::detail::writeU32(out, static_cast<std::uint32_t>(sections.size()));

  for (const auto &section : sections) {
    piper_neo::detail::writeString(out, section.name);
    piper_neo::detail::writeString(out, section.contentType);
    piper_neo::detail::writeU32(out, piper_neo::detail::NEO_COMPRESSION_NONE);
    piper_neo::detail::writeU64(out, section.payload.size());
    piper_neo::detail::writeU64(out, section.payload.size());
    piper_neo::detail::writeU64(out, section.offset);
  }

  for (const auto &section : sections) {
    out.write(section.payload.data(), static_cast<std::streamsize>(section.payload.size()));
  }
}

std::string readFile(const fs::path &path) {
  std::ifstream in(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void require(bool condition, const std::string &message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

} // namespace

int main(int argc, char **argv) {
  try {
    fs::path workDir = argc > 1 ? fs::path(argv[1]) : fs::temp_directory_path() / "piper-neo-test";
    fs::remove_all(workDir);
    fs::create_directories(workDir);

    const auto neoPath = workDir / "fixture.neo";
    writeFixturePackage(neoPath);

    require(piper_neo::isNeoFile(neoPath), "expected .neo extension");

    const auto info = piper_neo::inspectPackage(neoPath);
    require(info.version == 1, "invalid package version");
    require(info.modelBytes == 15, "invalid model size");
    require(info.modelCompression == "none", "invalid model compression");
    require(info.hasImage, "expected image section");
    require(info.imageContentType == "image/png", "invalid image content type");
    require(info.metadata["piper_neo"]["format"] == "piper-neo", "invalid metadata");

    const auto image = piper_neo::readImageSection(neoPath);
    require(image.first == "image/png", "image type mismatch");
    require(image.second == "fake-png-bytes", "image payload mismatch");

    const auto extracted = piper_neo::extractPackage(neoPath, workDir / "cache");
    require(fs::exists(extracted.modelPath), "model was not extracted");
    require(fs::exists(extracted.configPath), "config was not extracted");
    require(readFile(extracted.modelPath) == "fake-onnx-bytes", "extracted model mismatch");
    require(readFile(extracted.configPath).find("piper-neo") != std::string::npos,
            "extracted config mismatch");

    fs::remove_all(workDir);
    std::cout << "OK test_neo_package" << std::endl;
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
    return 1;
  }
}
