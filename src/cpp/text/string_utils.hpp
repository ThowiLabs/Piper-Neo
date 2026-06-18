#ifndef PIPER_TEXT_STRING_UTILS_HPP_
#define PIPER_TEXT_STRING_UTILS_HPP_

#include <cstddef>
#include <string>

namespace piper::textnorm {

std::string lowerAsciiCopy(std::string value);
std::string trimCopy(const std::string &value);

bool isAsciiAlphaNumericOrUnderscore(char c);
bool isWholeWordBoundaryBefore(const std::string &text, std::size_t index);
bool isWholeWordBoundaryAfter(const std::string &text, std::size_t index);
bool asciiStartsWithAt(const std::string &text, const std::string &needle,
                       std::size_t index, bool caseSensitive);

bool isTerminalPunctuation(char c);
std::string stripTerminalPunctuation(std::string &token);

} // namespace piper::textnorm

#endif // PIPER_TEXT_STRING_UTILS_HPP_
