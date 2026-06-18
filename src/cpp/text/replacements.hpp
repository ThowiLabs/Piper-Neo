#ifndef PIPER_TEXT_REPLACEMENTS_HPP_
#define PIPER_TEXT_REPLACEMENTS_HPP_

#include <string>
#include <vector>

#include "../text_normalizer.hpp"
#include "../json.hpp"

namespace piper::textnorm {

std::string applyCustomReplacements(const std::string &text,
                                    const std::vector<TextReplacementRule> &rules);
std::vector<TextReplacementRule> parseReplacementArray(const nlohmann::json &items);

} // namespace piper::textnorm

#endif // PIPER_TEXT_REPLACEMENTS_HPP_
