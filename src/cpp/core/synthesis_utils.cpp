#include "core/synthesis_utils.hpp"

#include <sstream>
#include <stdexcept>

namespace piper::core {

void throwIfSynthesisCancelled(const std::function<bool()> &shouldCancel) {
  if (shouldCancel && shouldCancel()) {
    throw std::runtime_error("synthesis_cancelled");
  }
}

std::string previewTextForLog(const std::string &text, std::size_t maxBytes) {
  if (text.size() <= maxBytes) {
    return text;
  }

  std::size_t end = maxBytes;
  while ((end > 0) &&
         ((static_cast<unsigned char>(text[end]) & 0xC0) == 0x80)) {
    --end;
  }

  if (end == 0) {
    end = maxBytes;
  }

  std::stringstream preview;
  preview << text.substr(0, end) << "... (" << text.size() << " byte(s))";
  return preview.str();
}

bool isAsciiWhitespace(char value) {
  return (value == ' ') || (value == '\n') || (value == '\r') ||
         (value == '\t') || (value == '\v') || (value == '\f');
}

bool startsWithBytes(const std::string &text, std::size_t index,
                     std::initializer_list<unsigned char> bytes) {
  if ((index + bytes.size()) > text.size()) {
    return false;
  }

  std::size_t offset = 0;
  for (auto expected : bytes) {
    if (static_cast<unsigned char>(text[index + offset]) != expected) {
      return false;
    }
    ++offset;
  }

  return true;
}

void trimLeadingAsciiWhitespace(const std::string &text, std::size_t &offset) {
  while ((offset < text.size()) && isAsciiWhitespace(text[offset])) {
    ++offset;
  }
}

void appendSilenceSamples(std::vector<int16_t> &audioBuffer,
                          std::size_t samples) {
  if (samples == 0) {
    return;
  }

  audioBuffer.insert(audioBuffer.end(), samples, 0);
}

std::size_t secondsToSamples(float seconds, int sampleRate, int channels) {
  if ((seconds <= 0.0f) || (sampleRate <= 0) || (channels <= 0)) {
    return 0;
  }

  return static_cast<std::size_t>(seconds * sampleRate * channels);
}

} // namespace piper::core
