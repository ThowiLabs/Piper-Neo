#include "text/builtin_normalizer.hpp"
#include "text/builtin_renderers.hpp"
#include "text/protected_segments.hpp"
#include "text_normalizer.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void requireEqual(const std::string &name, const std::string &got,
                  const std::string &expected) {
  if (got != expected) {
    std::cerr << "FAIL: " << name << "
expected: " << expected
              << "
got:      " << got << std::endl;
    std::exit(1);
  }
}

piper::TextNormalizationBuiltinConfig safeBuiltins() {
  piper::TextNormalizationBuiltinConfig cfg;
  cfg.decimals = true;     // compatibilidad: se ignora
  cfg.versions = true;     // compatibilidad: se ignora
  cfg.percentages = true;  // compatibilidad: se ignora
  cfg.currency = true;     // compatibilidad: se ignora
  cfg.urls = true;
  cfg.emails = true;
  return cfg;
}

std::string normalizeBuiltinsOnly(const std::string &text) {
  auto result = piper::textnorm::normalizeBuiltins(text, safeBuiltins());
  return piper::textnorm::restoreProtectedSegments(result.text,
                                                   result.protectedSegments);
}

} // namespace

int main() {
  using namespace piper::textnorm;

  requireEqual("email", emailToSpeechText("soporte-test+mx@demo.com"),
               "soporte guion test más mx arroba demo punto com");
  requireEqual("url", urlToSpeechText("https://github.com/rhasspy/piper"),
               "github punto com diagonal rhasspy diagonal piper");

  requireEqual("url/email only",
               normalizeBuiltinsOnly("v1.2.3 cuesta $99.50 pesos, 13.5%, https://x.com y a@b.mx"),
               "v1.2.3 cuesta $99.50 pesos, 13.5%, x punto com y a arroba b punto mx");

  requireEqual("url trailing punctuation",
               normalizeBuiltinsOnly("Visita https://youtube.com."),
               "Visita youtube punto com.");

  requireEqual("numbers untouched",
               normalizeBuiltinsOnly("abc123.45def 123.45 2026 13% $99.50"),
               "abc123.45def 123.45 2026 13% $99.50");

  std::cout << "TEST_OK" << std::endl;
  return 0;
}
