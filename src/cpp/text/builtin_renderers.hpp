#ifndef PIPER_TEXT_BUILTIN_RENDERERS_HPP_
#define PIPER_TEXT_BUILTIN_RENDERERS_HPP_

#include <string>

namespace piper::textnorm {

std::string emailToSpeechText(const std::string &email);
std::string urlToSpeechText(const std::string &url);

} // namespace piper::textnorm

#endif // PIPER_TEXT_BUILTIN_RENDERERS_HPP_
