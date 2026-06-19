#include "builtin_renderers.hpp"

#include "string_utils.hpp"

namespace piper::textnorm {

std::string emailToSpeechText(const std::string &email) {
  std::string out;
  for (char c : email) {
    if (c == '@') {
      out += " arroba ";
    } else if (c == '.') {
      out += " punto ";
    } else if (c == '_') {
      out += " guion bajo ";
    } else if (c == '-') {
      out += " guion ";
    } else if (c == '+') {
      out += " más ";
    } else {
      out.push_back(c);
    }
  }
  return out;
}

std::string urlToSpeechText(const std::string &url) {
  std::string value = url;
  const auto lower = lowerAsciiCopy(value);
  if (lower.rfind("https://", 0) == 0) {
    value = value.substr(8);
  } else if (lower.rfind("http://", 0) == 0) {
    value = value.substr(7);
  } else if (lower.rfind("www.", 0) == 0) {
    value = value.substr(4);
  }

  std::string out;
  for (char c : value) {
    if (c == '.') {
      out += " punto ";
    } else if (c == '/') {
      out += " diagonal ";
    } else if (c == '-') {
      out += " guion ";
    } else if (c == '_') {
      out += " guion bajo ";
    } else if (c == '?') {
      out += " signo de pregunta ";
    } else if (c == '&') {
      out += " y ";
    } else if (c == '=') {
      out += " igual ";
    } else if (c == ':') {
      out += " dos puntos ";
    } else {
      out.push_back(c);
    }
  }
  return out;
}

} // namespace piper::textnorm
