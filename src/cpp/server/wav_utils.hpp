#ifndef PIPER_SERVER_WAV_UTILS_H_
#define PIPER_SERVER_WAV_UTILS_H_

#include <cstdint>
#include <filesystem>
#include <ostream>
#include <string>

namespace piper_server {

struct WavPayload {
  std::string pcm;
  int sampleRate = 0;
  int sampleWidth = 0;
  int channels = 0;
};

void writeServerWavHeader(int sampleRate, int sampleWidth, int channels,
                          std::uint32_t dataSizeBytes,
                          std::ostream &audioFile);
WavPayload readServerWavPayload(const std::filesystem::path &path);
std::uint64_t silenceBytesForMs(std::uint64_t ms, int sampleRate, int sampleWidth, int channels);
void writeZeroBytes(std::ostream &out, std::uint64_t bytes);
std::string resamplePcmS16Linear(const std::string &pcm, int sourceSampleRate, int targetSampleRate,
                                 int sampleWidth, int channels);

} // namespace piper_server

#endif // PIPER_SERVER_WAV_UTILS_H_
