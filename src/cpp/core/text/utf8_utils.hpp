#ifndef PIPER_CORE_TEXT_UTF8_UTILS_HPP_
#define PIPER_CORE_TEXT_UTF8_UTILS_HPP_

#include <cstddef>
#include <initializer_list>
#include <string>

namespace piper::text {

bool isUtf8ContinuationByte(char value);
std::size_t clampToUtf8Boundary(const std::string &text, std::size_t index);
bool isAsciiWhitespace(char value);
bool startsWithBytes(const std::string &text, std::size_t index,
                     std::initializer_list<unsigned char> bytes);
void trimLeadingAsciiWhitespace(const std::string &text, std::size_t &offset);

} // namespace piper::text

#endif // PIPER_CORE_TEXT_UTF8_UTILS_HPP_
