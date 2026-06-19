#ifndef PIPER_TEXT_BUILTIN_RENDERERS_HPP_
#define PIPER_TEXT_BUILTIN_RENDERERS_HPP_

#include <string>

namespace piper::textnorm {

std::string versionToSpanish(const std::string &version);
std::string emailToSpanish(const std::string &email);
std::string urlToSpanish(const std::string &url);
std::string currencyToSpanish(const std::string &currencyRaw,
                              const std::string &suffixRaw,
                              const std::string &integerPart,
                              const std::string &fractionPart);
std::string percentageToSpanish(const std::string &integerPart,
                                const std::string &fractionPart);

} // namespace piper::textnorm

#endif // PIPER_TEXT_BUILTIN_RENDERERS_HPP_
