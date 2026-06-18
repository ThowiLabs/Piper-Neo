#include "wav_utils.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <string>

namespace piper_server {
namespace {

struct PcmWavHeader {
  uint8_t RIFF[4] = {'R', 'I', 'F', 'F'};
  uint32_t chunkSize = 0;
  uint8_t WAVE[4] = {'W', 'A', 'V', 'E'};
  uint8_t fmt[4] = {'f', 'm', 't', ' '};
  uint32_t fmtSize = 16;
  uint16_t audioFormat = 1;
  uint16_t numChannels = 1;
  uint32_t sampleRate = 22050;
  uint32_t bytesPerSec = 44100;
  uint16_t blockAlign = 2;
  uint16_t bitsPerSample = 16;
  uint8_t data[4] = {'d', 'a', 't', 'a'};
  uint32_t dataSize = 0;
};

} // namespace

void writeServerWavHeader(int sampleRate, int sampleWidth, int channels,
                          std::uint32_t dataSizeBytes,
                          std::ostream &audioFile) {
  PcmWavHeader header;
  header.dataSize = dataSizeBytes;
  header.chunkSize = header.dataSize + sizeof(PcmWavHeader) - 8;
  header.sampleRate = static_cast<uint32_t>(sampleRate);
  header.numChannels = static_cast<uint16_t>(channels);
  header.bytesPerSec = static_cast<uint32_t>(sampleRate * sampleWidth * channels);
  header.blockAlign = static_cast<uint16_t>(sampleWidth * channels);
  header.bitsPerSample = static_cast<uint16_t>(sampleWidth * 8);
  audioFile.write(reinterpret_cast<const char *>(&header), sizeof(header));
}


std::uint16_t readLe16(const char *ptr) {
  return static_cast<std::uint16_t>(static_cast<unsigned char>(ptr[0])) |
         static_cast<std::uint16_t>(static_cast<unsigned char>(ptr[1]) << 8);
}

std::uint32_t readLe32(const char *ptr) {
  return static_cast<std::uint32_t>(static_cast<unsigned char>(ptr[0])) |
         (static_cast<std::uint32_t>(static_cast<unsigned char>(ptr[1])) << 8) |
         (static_cast<std::uint32_t>(static_cast<unsigned char>(ptr[2])) << 16) |
         (static_cast<std::uint32_t>(static_cast<unsigned char>(ptr[3])) << 24);
}

WavPayload readServerWavPayload(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file.good()) {
    throw std::runtime_error("markup_missing_segment_wav");
  }

  std::string header(44, '\0');
  file.read(header.data(), static_cast<std::streamsize>(header.size()));
  if (file.gcount() != static_cast<std::streamsize>(header.size()) ||
      header.compare(0, 4, "RIFF") != 0 || header.compare(8, 4, "WAVE") != 0 ||
      header.compare(12, 4, "fmt ") != 0 || header.compare(36, 4, "data") != 0) {
    throw std::runtime_error("markup_invalid_segment_wav");
  }

  WavPayload payload;
  payload.channels = static_cast<int>(readLe16(header.data() + 22));
  payload.sampleRate = static_cast<int>(readLe32(header.data() + 24));
  payload.sampleWidth = static_cast<int>(readLe16(header.data() + 34) / 8);
  const auto dataSize = readLe32(header.data() + 40);
  payload.pcm.resize(dataSize);
  file.read(payload.pcm.data(), static_cast<std::streamsize>(payload.pcm.size()));
  if (file.gcount() != static_cast<std::streamsize>(payload.pcm.size())) {
    throw std::runtime_error("markup_truncated_segment_wav");
  }
  return payload;
}


std::uint64_t silenceBytesForMs(std::uint64_t ms, int sampleRate, int sampleWidth, int channels) {
  const std::uint64_t blockAlign = static_cast<std::uint64_t>(std::max(1, sampleWidth) * std::max(1, channels));
  const std::uint64_t bytes = (static_cast<std::uint64_t>(sampleRate) * blockAlign * ms) / 1000;
  return bytes - (bytes % blockAlign);
}

void writeZeroBytes(std::ostream &out, std::uint64_t bytes) {
  std::array<char, 64 * 1024> zero{};
  while (bytes > 0) {
    const auto count = static_cast<std::size_t>(std::min<std::uint64_t>(bytes, zero.size()));
    out.write(zero.data(), static_cast<std::streamsize>(count));
    bytes -= count;
  }
}


void writeLe16ToString(std::string &out, std::size_t offset, std::int32_t value) {
  const auto clamped = std::max<std::int32_t>(std::numeric_limits<std::int16_t>::min(),
                                             std::min<std::int32_t>(std::numeric_limits<std::int16_t>::max(), value));
  const auto sample = static_cast<std::uint16_t>(static_cast<std::int16_t>(clamped));
  out[offset] = static_cast<char>(sample & 0xFF);
  out[offset + 1] = static_cast<char>((sample >> 8) & 0xFF);
}

std::string resamplePcmS16Linear(const std::string &pcm, int sourceSampleRate, int targetSampleRate,
                                 int sampleWidth, int channels) {
  if (sourceSampleRate == targetSampleRate) {
    return pcm;
  }
  if (sourceSampleRate <= 0 || targetSampleRate <= 0 || channels <= 0 || sampleWidth != 2) {
    throw std::runtime_error("markup_audio_resample_failed");
  }

  const auto blockAlign = static_cast<std::size_t>(sampleWidth * channels);
  if (blockAlign == 0 || (pcm.size() % blockAlign) != 0) {
    throw std::runtime_error("markup_audio_resample_failed");
  }

  const auto sourceFrames = pcm.size() / blockAlign;
  if (sourceFrames == 0) {
    return {};
  }

  const auto targetFrames = std::max<std::size_t>(
      1, static_cast<std::size_t>((static_cast<long double>(sourceFrames) *
                                   static_cast<long double>(targetSampleRate) /
                                   static_cast<long double>(sourceSampleRate)) + 0.5L));
  std::string out(targetFrames * blockAlign, '\0');

  auto sampleAt = [&](std::size_t frame, int channel) -> std::int16_t {
    const auto offset = (frame * static_cast<std::size_t>(channels) + static_cast<std::size_t>(channel)) * 2;
    return static_cast<std::int16_t>(readLe16(pcm.data() + offset));
  };

  for (std::size_t frame = 0; frame < targetFrames; ++frame) {
    const long double sourcePosition = (static_cast<long double>(frame) *
                                        static_cast<long double>(sourceSampleRate)) /
                                       static_cast<long double>(targetSampleRate);
    auto baseFrame = static_cast<std::size_t>(sourcePosition);
    if (baseFrame >= sourceFrames) {
      baseFrame = sourceFrames - 1;
    }
    const auto nextFrame = std::min<std::size_t>(baseFrame + 1, sourceFrames - 1);
    const long double fraction = sourcePosition - static_cast<long double>(baseFrame);

    for (int channel = 0; channel < channels; ++channel) {
      const auto a = static_cast<long double>(sampleAt(baseFrame, channel));
      const auto b = static_cast<long double>(sampleAt(nextFrame, channel));
      const auto interpolated = static_cast<std::int32_t>(a + ((b - a) * fraction));
      const auto offset = (frame * static_cast<std::size_t>(channels) + static_cast<std::size_t>(channel)) * 2;
      writeLe16ToString(out, offset, interpolated);
    }
  }

  return out;
}

} // namespace piper_server
