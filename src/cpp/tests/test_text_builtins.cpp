#include "text/builtin_normalizer.hpp"
#include "text/builtin_renderers.hpp"
#include "text/protected_segments.hpp"
#include "text/spanish_numbers.hpp"
#include "text_normalizer.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void requireEqual(const std::string &name, const std::string &got,
                  const std::string &expected) {
  if (got != expected) {
    std::cerr << "FAIL: " << name << "\nexpected: " << expected
              << "\ngot:      " << got << std::endl;
    std::exit(1);
  }
}

piper::TextNormalizationBuiltinConfig allBuiltins() {
  piper::TextNormalizationBuiltinConfig cfg;
  cfg.decimals = true;
  cfg.versions = true;
  cfg.percentages = true;
  cfg.currency = true;
  cfg.urls = true;
  cfg.emails = true;
  return cfg;
}

std::string normalizeBuiltinsOnly(const std::string &text) {
  auto result = piper::textnorm::normalizeBuiltins(text, allBuiltins());
  return piper::textnorm::restoreProtectedSegments(result.text,
                                                   result.protectedSegments);
}

} // namespace

int main() {
  using namespace piper::textnorm;

  requireEqual("integer", integerToSpanish(1234567),
               "un millón doscientos treinta y cuatro mil quinientos sesenta y siete");
  requireEqual("leading zeros", numericGroupToSpanish("007"), "cero cero siete");
  requireEqual("decimal", decimalToSpanish("3", "05"), "tres punto cero cinco");

  requireEqual("version", versionToSpanish("v1.2.03"),
               "versión uno punto dos punto cero tres");
  requireEqual("email", emailToSpanish("soporte-test+mx@demo.com"),
               "soporte guion test más mx arroba demo punto com");
  requireEqual("url", urlToSpanish("https://github.com/rhasspy/piper"),
               "github punto com diagonal rhasspy diagonal piper");
  requireEqual("currency mxn", currencyToSpanish("$", "pesos", "99", "50"),
               "99 punto 50 pesos");
  requireEqual("currency usd", currencyToSpanish("USD ", "", "12", ""),
               "12 dólares");
  requireEqual("percent", percentageToSpanish("13", "5"), "13 punto 5 por ciento");

  requireEqual("full builtins",
               normalizeBuiltinsOnly("v1.2.3 cuesta $99.50 pesos, 13.5%, https://x.com y a@b.mx"),
               "versión uno punto dos punto tres cuesta 99 punto 50 pesos, 13 punto 5 por ciento, x punto com y a arroba b punto mx");

  auto protectedResult = normalizeBuiltinsOnly("Visita https://youtube.com.");
  requireEqual("url trailing punctuation", protectedResult,
               "Visita youtube punto com.");

  auto partial = normalizeBuiltins("abc123.45def 123.45", allBuiltins());
  auto restored = restoreProtectedSegments(partial.text, partial.protectedSegments);
  requireEqual("safe decimal boundaries", restored, "abc123.45def ciento veintitres punto cuarenta y cinco");

  std::cout << "TEST_OK" << std::endl;
  return 0;
}
