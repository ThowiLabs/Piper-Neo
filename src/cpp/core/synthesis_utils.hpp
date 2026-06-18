#ifndef PIPER_CORE_SYNTHESIS_UTILS_HPP_
#define PIPER_CORE_SYNTHESIS_UTILS_HPP_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <string>
#include <vector>

namespace piper::core {

void throwIfSynthesisCancelled(const std::function<bool()> &shouldCancel);

std::string previewTextForLog(const std::string &text,
                              std::size_t maxBytes = 256);

bool isAsciiWhitespace(char value);

bool startsWithBytes(const std::string &text, std::size_t index,
                     std::initializer_list<unsigned char> bytes);

void trimLeadingAsciiWhitespace(const std::string &text, std::size_t &offset);

void appendSilenceSamples(std::vector<int16_t> &audioBuffer,
                          std::size_t samples);

std::size_t secondsToSamples(float seconds, int sampleRate, int channels);

} // namespace piper::core

#endif // PIPER_CORE_SYNTHESIS_UTILS_HPP_
