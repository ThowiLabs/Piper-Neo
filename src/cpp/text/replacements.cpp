#include "replacements.hpp"

#include <algorithm>
#include <utility>

#include "string_utils.hpp"

namespace piper::textnorm {

namespace {

std::string applyReplacementRule(const std::string &text,
                                 const TextReplacementRule &rule) {
  if (rule.from.empty()) {
    return text;
  }

  std::string out;
  out.reserve(text.size());

  std::size_t i = 0;
  while (i < text.size()) {
    const bool boundaryOk = !rule.wholeWord ||
                            (isWholeWordBoundaryBefore(text, i) &&
                             isWholeWordBoundaryAfter(text, i + rule.from.size()));
    if (boundaryOk && asciiStartsWithAt(text, rule.from, i, rule.caseSensitive)) {
      out += rule.to;
      i += rule.from.size();
      continue;
    }

    out.push_back(text[i]);
    ++i;
  }

  return out;
}

} // namespace

std::string applyCustomReplacements(const std::string &text,
                                    const std::vector<TextReplacementRule> &rules) {
  if (rules.empty()) {
    return text;
  }

  auto sortedRules = rules;
  std::stable_sort(sortedRules.begin(), sortedRules.end(),
                   [](const TextReplacementRule &a, const TextReplacementRule &b) {
                     if (a.priority != b.priority) {
                       return a.priority > b.priority;
                     }
                     return a.from.size() > b.from.size();
                   });

  std::string current = text;
  for (const auto &rule : sortedRules) {
    current = applyReplacementRule(current, rule);
  }

  return current;
}

std::vector<TextReplacementRule> parseReplacementArray(const nlohmann::json &items) {
  std::vector<TextReplacementRule> rules;
  if (!items.is_array()) {
    return rules;
  }

  for (const auto &item : items) {
    TextReplacementRule rule;
    if (item.is_object()) {
      rule.from = item.value("from", "");
      rule.to = item.value("to", "");
      rule.caseSensitive = item.value("case_sensitive", false);
      rule.wholeWord = item.value("whole_word", true);
      rule.priority = item.value("priority", 0);
      rule.note = item.value("note", "");
    } else if (item.is_array() && item.size() >= 2 && item[0].is_string() && item[1].is_string()) {
      rule.from = item[0].get<std::string>();
      rule.to = item[1].get<std::string>();
    }

    rule.from = trimCopy(rule.from);
    if (!rule.from.empty()) {
      rules.push_back(std::move(rule));
    }
  }

  return rules;
}

} // namespace piper::textnorm
