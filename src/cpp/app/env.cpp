#include "env.hpp"

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace piper_app {

using namespace std;

string trimEnvValue(string value) {
  auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
  while (!value.empty() && isSpace(static_cast<unsigned char>(value.front()))) {
    value.erase(value.begin());
  }
  while (!value.empty() && isSpace(static_cast<unsigned char>(value.back()))) {
    value.pop_back();
  }

  if (value.size() >= 2) {
    const char first = value.front();
    const char last = value.back();
    if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
      value = value.substr(1, value.size() - 2);
    }
  }

  return value;
}

optional<string> envValue(const string &key) {
  const char *rawValue = std::getenv(key.c_str());
  if (rawValue == nullptr) {
    return nullopt;
  }

  auto value = trimEnvValue(rawValue);
  if (value.empty()) {
    return nullopt;
  }

  return value;
}

optional<string> dotenvValue(const filesystem::path &envPath,
                             const vector<string> &keys) {
  ifstream envFile(envPath.string());
  if (!envFile.good()) {
    return nullopt;
  }

  string line;
  while (getline(envFile, line)) {
    line = trimEnvValue(line);
    if (line.empty() || line[0] == '#') {
      continue;
    }

    const auto equals = line.find('=');
    if (equals == string::npos) {
      continue;
    }

    auto key = trimEnvValue(line.substr(0, equals));
    auto value = trimEnvValue(line.substr(equals + 1));
    if (!value.empty() && value[0] == '#') {
      continue;
    }

    for (const auto &expectedKey : keys) {
      if (key == expectedKey && !value.empty()) {
        return value;
      }
    }
  }

  return nullopt;
}

optional<string> resolveApiToken(const optional<string> &explicitToken) {
  if (explicitToken && !explicitToken->empty()) {
    return explicitToken;
  }

  const vector<string> keys = {"PIPER_API_TOKEN", "PIPER_AUTH_TOKEN", "API_TOKEN"};
  for (const auto &key : keys) {
    if (auto value = envValue(key)) {
      return value;
    }
  }

  return dotenvValue(filesystem::path(".env"), keys);
}

} // namespace piper_app
