#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>

#include "server/text_sanitizer.hpp"

namespace {

void require(bool condition, const std::string &message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

bool hasWarning(const piper_server::TtsTextSanitizeResult &result, const std::string &warning) {
  return std::find(result.warnings.begin(), result.warnings.end(), warning) != result.warnings.end();
}

piper_server::TtsTextSanitizeResult sanitize(const std::string &text, std::size_t maxChars = 500) {
  piper_server::TtsTextSanitizeResult result;
  piper_server::sanitizeTtsTextForApi(text, maxChars, result);
  return result;
}

} // namespace

int main() {
  try {
    {
      const auto result = sanitize("  Hola\t mundo  ");
      require(result.ok, "plain text should be accepted");
      require(result.speakText == "Hola mundo", "whitespace should be normalized");
      require(hasWarning(result, "TEXT_TRIMMED"), "trim warning expected");
      require(hasWarning(result, "WHITESPACE_NORMALIZED"), "whitespace warning expected");
    }

    {
      const auto result = sanitize("Visita https://github.com o escribe a demo@example.com");
      require(result.ok, "urls and emails should be accepted");
      require(result.urls >= 1, "url should be counted");
      require(result.emails == 1, "email should be counted");
      require(result.speakText.find("enlace a") == std::string::npos,
              "URL must not be rewritten by API sanitizer");
      require(result.speakText.find("correo electronico") == std::string::npos,
              "email must not be rewritten by API sanitizer");
    }

    {
      const auto result = sanitize("<p>Hola<br>mundo</p> **bien** [link](https://example.com)");
      require(result.ok, "markup text should be accepted");
      require(hasWarning(result, "MARKUP_STRIPPED"), "markup warning expected");
      require(hasWarning(result, "MARKDOWN_STRIPPED"), "markdown warning expected");
      require(result.speakText.find('<') == std::string::npos, "html tags should be stripped");
    }

    {
      const auto result = sanitize("```js\nconst token = 'abc';\n```\nTexto normal");
      require(result.ok, "fenced code should be summarized");
      require(result.codeBlocks == 1, "one code block expected");
      require(hasWarning(result, "CODE_SUMMARIZED"), "code warning expected");
      require(result.speakText.find("bloque de codigo omitido") != std::string::npos,
              "fenced code should be replaced");
    }

    {
      const auto result = sanitize("Hola 😀😀😀😀😀");
      require(result.ok, "emoji text should be accepted");
      require(result.emojis == 5, "emojis should be counted");
      require(hasWarning(result, "EMOJI_SUMMARIZED"), "emoji warning expected");
      require(result.speakText.find("emojis omitidos") != std::string::npos,
              "long emoji run should be summarized");
    }

    {
      const auto result = sanitize(std::string(100, 'A'), 10);
      require(result.ok, "long text should be clipped instead of rejected");
      require(result.speakChars == 10, "text should be clipped by UTF-8 chars");
      require(hasWarning(result, "TEXT_TOO_LONG"), "length warning expected");
    }

    {
      piper_server::TtsTextSanitizeResult result;
      const std::string invalid{static_cast<char>(0xC3), static_cast<char>(0x28)};
      piper_server::sanitizeTtsTextForApi(invalid, 20, result);
      require(!result.ok, "invalid UTF-8 should be rejected");
      require(hasWarning(result, "INVALID_UTF8"), "invalid UTF-8 warning expected");
      require(result.riskScore == 1.0, "invalid UTF-8 should be high risk");
    }

    std::cout << "OK test_text_sanitizer" << std::endl;
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
    return 1;
  }
}
