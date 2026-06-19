#ifndef PIPER_TEXT_BUILTIN_MATCHERS_HPP_
#define PIPER_TEXT_BUILTIN_MATCHERS_HPP_

#include <regex>
#include <string>

namespace piper::textnorm {

struct BuiltinPatterns {
  std::regex url;
  std::regex email;

  BuiltinPatterns();
};

const BuiltinPatterns &builtinPatterns();
bool prefixRegexMatch(const std::string &text, std::size_t index,
                      const std::regex &regex, std::smatch &match);
bool isSafeLeftBoundary(const std::string &text, std::size_t index);
bool isSafeRightBoundary(const std::string &text, std::size_t index);

} // namespace piper::textnorm

#endif // PIPER_TEXT_BUILTIN_MATCHERS_HPP_
