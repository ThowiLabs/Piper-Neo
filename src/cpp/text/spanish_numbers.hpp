#ifndef PIPER_TEXT_SPANISH_NUMBERS_HPP_
#define PIPER_TEXT_SPANISH_NUMBERS_HPP_

#include <string>
#include <vector>

namespace piper::textnorm {

std::string joinWords(const std::vector<std::string> &words,
                      const std::string &sep = " ");
std::string digitToSpanish(char digit);
std::string digitsToSpanish(const std::string &digits);
std::string integerToSpanish(long long value);
std::string numericGroupToSpanish(const std::string &digits);
std::string decimalToSpanish(const std::string &integerPart,
                             const std::string &fractionPart);
std::string decimalDigitsToSpeechText(const std::string &integerPart,
                                      const std::string &fractionPart);

} // namespace piper::textnorm

#endif // PIPER_TEXT_SPANISH_NUMBERS_HPP_
