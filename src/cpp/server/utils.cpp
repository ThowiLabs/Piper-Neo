#include "utils.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <mutex>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>

namespace piper_server {

std::string nowIso8601() {
  const auto now = std::chrono::system_clock::now();
  const auto time = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
#ifdef _WIN32
  gmtime_s(&tm, &time);
#else
  gmtime_r(&time, &tm);
#endif
  std::ostringstream out;
  out << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
  return out.str();
}

std::string lowerCopy(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return value;
}

std::string trimCopy(const std::string &value) {
  std::size_t begin = 0;
  while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin]))) {
    ++begin;
  }

  std::size_t end = value.size();
  while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
    --end;
  }

  return value.substr(begin, end - begin);
}

bool isSafeFileName(const std::string &fileName) {
  if (fileName.empty() || fileName == "." || fileName == "..") {
    return false;
  }

  for (char c : fileName) {
    const auto value = static_cast<unsigned char>(c);
    if (!(std::isalnum(value) || c == '_' || c == '-' || c == '.')) {
      return false;
    }
  }

  return (fileName.find("..") == std::string::npos) &&
         (fileName.find('/') == std::string::npos) &&
         (fileName.find('\\') == std::string::npos);
}

std::string randomSuffix() {
  static std::mutex randomMutex;
  static std::mt19937_64 rng{std::random_device{}()};
  std::lock_guard<std::mutex> lock(randomMutex);
  std::uniform_int_distribution<unsigned long long> dist;
  std::ostringstream out;
  out << std::hex << dist(rng);
  return out.str().substr(0, 10);
}

std::string makeOutputFileName() {
  const auto now = std::chrono::system_clock::now();
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      now.time_since_epoch())
                      .count();
  std::ostringstream name;
  name << "tts_" << ms << "_" << randomSuffix() << ".wav";
  return name.str();
}

json loadJsonFile(const std::filesystem::path &path) {
  std::ifstream file(path.string(), std::ios::binary);
  if (!file.good()) {
    throw std::runtime_error("not_found");
  }
  json root;
  file >> root;
  return root;
}

std::optional<json> tryLoadJsonFile(const std::filesystem::path &path,
                                    std::string &error) {
  try {
    return loadJsonFile(path);
  } catch (const json::exception &) {
    error = "invalid_json";
  } catch (const std::exception &e) {
    error = e.what();
  }
  return std::nullopt;
}

const std::string BASE64_CHARS =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string decodeBase64(const std::string &encoded) {
  std::array<int, 256> table{};
  table.fill(-1);
  for (int i = 0; i < static_cast<int>(BASE64_CHARS.size()); ++i) {
    table[static_cast<unsigned char>(BASE64_CHARS[i])] = i;
  }

  std::string decoded;
  int val = 0;
  int valb = -8;
  for (unsigned char c : encoded) {
    if (std::isspace(c)) {
      continue;
    }
    if (c == '=') {
      break;
    }
    if (table[c] == -1) {
      throw std::runtime_error("invalid_image");
    }
    val = (val << 6) + table[c];
    valb += 6;
    if (valb >= 0) {
      decoded.push_back(static_cast<char>((val >> valb) & 0xFF));
      valb -= 8;
    }
  }
  return decoded;
}

std::pair<std::string, std::string> parseDataImage(const std::string &dataUri) {
  const std::string prefix = "data:image/";
  if (dataUri.rfind(prefix, 0) != 0) {
    throw std::runtime_error("invalid_image");
  }
  const auto comma = dataUri.find(',');
  if (comma == std::string::npos) {
    throw std::runtime_error("invalid_image");
  }
  const auto meta = dataUri.substr(0, comma);
  const auto lowerMeta = lowerCopy(meta);
  if (lowerMeta.find(";base64") == std::string::npos) {
    throw std::runtime_error("invalid_image");
  }

  std::string contentType;
  if (lowerMeta.rfind("data:image/jpeg", 0) == 0 || lowerMeta.rfind("data:image/jpg", 0) == 0) {
    contentType = "image/jpeg";
  } else if (lowerMeta.rfind("data:image/png", 0) == 0) {
    contentType = "image/png";
  } else if (lowerMeta.rfind("data:image/webp", 0) == 0) {
    contentType = "image/webp";
  } else {
    throw std::runtime_error("invalid_image");
  }

  return {contentType, decodeBase64(dataUri.substr(comma + 1))};
}

} // namespace piper_server
