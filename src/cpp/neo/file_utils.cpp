#include "neo/file_utils.hpp"

#include <cctype>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace piper_neo::detail {

std::string lowerCopy(std::string value) {
  for (auto &c : value) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return value;
}

std::vector<char> readFileBytes(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file.good()) {
    throw std::runtime_error("file_not_found");
  }

  file.seekg(0, std::ios::end);
  const auto size = file.tellg();
  if (size < 0) {
    throw std::runtime_error("file_read_error");
  }

  file.seekg(0, std::ios::beg);
  std::vector<char> bytes(static_cast<std::size_t>(size));
  if (!bytes.empty()) {
    file.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  }
  return bytes;
}

void writeFileBytes(const std::filesystem::path &path, const std::vector<char> &bytes) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream file(path, std::ios::binary);
  if (!file.good()) {
    throw std::runtime_error("file_write_error");
  }
  if (!bytes.empty()) {
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  }
}

std::string fnv1aHex(const std::string &input) {
  std::uint64_t hash = 1469598103934665603ULL;
  for (unsigned char c : input) {
    hash ^= c;
    hash *= 1099511628211ULL;
  }

  std::ostringstream out;
  out << std::hex << std::setw(16) << std::setfill('0') << hash;
  return out.str();
}

} // namespace piper_neo::detail
