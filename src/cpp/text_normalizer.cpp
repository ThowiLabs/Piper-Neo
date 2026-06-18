#include "text_normalizer.hpp"

#include <exception>

#include "text/builtin_normalizer.hpp"
#include "text/replacements.hpp"

namespace piper {
namespace {

void parseBuiltinFlags(const nlohmann::json &root,
                       TextNormalizationBuiltinConfig &builtin) {
  if (!root.is_object()) {
    return;
  }

  builtin.decimals = root.value("decimals", builtin.decimals);
  builtin.versions = root.value("versions", builtin.versions);
  builtin.percentages = root.value("percentages", builtin.percentages);
  builtin.currency = root.value("currency", builtin.currency);
  builtin.urls = root.value("urls", builtin.urls);
  builtin.emails = root.value("emails", builtin.emails);
}

void appendParsedReplacements(const nlohmann::json &items,
                              std::vector<TextReplacementRule> &rules) {
  auto parsed = textnorm::parseReplacementArray(items);
  rules.insert(rules.end(), parsed.begin(), parsed.end());
}

} // namespace

void parseTextNormalizationConfig(const nlohmann::json &configRoot,
                                  TextNormalizationConfig &config) {
  // A classic Piper JSON must not be modified implicitly. Normalization is
  // enabled only when the model explicitly declares neo.text_normalization, or
  // when it carries legacy modelcard.replacements from older managers.
  config = TextNormalizationConfig{};

  try {
    bool hasNeoTextNormalization = false;

    if (configRoot.contains("neo") && configRoot["neo"].is_object()) {
      const auto &neo = configRoot["neo"];
      if (neo.contains("text_normalization") && neo["text_normalization"].is_object()) {
        hasNeoTextNormalization = true;
        const auto &tn = neo["text_normalization"];

        config.enabled = tn.value("enabled", true);
        config.locale = tn.value("locale", config.locale);

        // Piper Neo no aplica reglas inteligentes por defecto. Cada modelo
        // debe declarar explicitamente que tipo de normalizacion quiere usar
        // en neo.text_normalization.builtin. Esto evita que el core convierta
        // URLs, dominios, moneda o porcentajes sin control del modelo.
        if (tn.contains("builtin")) {
          parseBuiltinFlags(tn["builtin"], config.builtin);
        }

        if (tn.contains("replacements")) {
          appendParsedReplacements(tn["replacements"], config.replacements);
        }
      }
    }

    // Legacy managers used modelcard.replacements as [[from, to], ...]. Keep it
    // supported, but do not enable smart builtins unless neo.text_normalization
    // exists. This prevents old Piper JSON files from changing unexpectedly.
    if (configRoot.contains("modelcard") && configRoot["modelcard"].is_object()) {
      const auto &card = configRoot["modelcard"];
      if (card.contains("replacements")) {
        auto parsed = textnorm::parseReplacementArray(card["replacements"]);
        if (!parsed.empty() && !hasNeoTextNormalization) {
          config.enabled = true;
        }
        config.replacements.insert(config.replacements.end(), parsed.begin(), parsed.end());
      }
    }
  } catch (const std::exception &) {
    config = TextNormalizationConfig{};
  }
}

std::string normalizeTextForSpeech(const std::string &text,
                                   const TextNormalizationConfig &config) {
  if (!config.enabled || text.empty()) {
    return text;
  }

  auto builtinResult = textnorm::normalizeBuiltins(text, config.builtin);
  auto normalized = textnorm::applyCustomReplacements(builtinResult.text,
                                                      config.replacements);
  return textnorm::restoreProtectedSegments(normalized,
                                            builtinResult.protectedSegments);
}

} // namespace piper
