#ifndef PIPER_TEXT_BUILTIN_NORMALIZER_HPP_
#define PIPER_TEXT_BUILTIN_NORMALIZER_HPP_

#include <string>
#include <vector>

#include "../text_normalizer.hpp"

namespace piper::textnorm {

struct BuiltinNormalizationResult {
  std::string text;
  std::vector<std::string> protectedSegments;
};

BuiltinNormalizationResult normalizeBuiltins(
    const std::string &text, const TextNormalizationBuiltinConfig &builtin);

std::string restoreProtectedSegments(const std::string &text,
                                     const std::vector<std::string> &segments);

} // namespace piper::textnorm

#endif // PIPER_TEXT_BUILTIN_NORMALIZER_HPP_
