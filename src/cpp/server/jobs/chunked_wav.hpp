#ifndef PIPER_SERVER_JOBS_CHUNKED_WAV_H_
#define PIPER_SERVER_JOBS_CHUNKED_WAV_H_

#include <cstdint>
#include <filesystem>
#include <vector>

namespace piper_server {

struct ChunkedWavOutput {
  std::filesystem::path outputPath;
  std::vector<std::filesystem::path> chunkPaths;
  std::vector<std::uintmax_t> chunkBytes;
  int sampleRate = 22050;
  int sampleWidth = 2;
  int channels = 1;
};

void assembleChunkedWav(const ChunkedWavOutput &job);

} // namespace piper_server

#endif // PIPER_SERVER_JOBS_CHUNKED_WAV_H_
