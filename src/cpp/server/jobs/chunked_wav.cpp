#include "server/jobs/chunked_wav.hpp"

#include <array>
#include <fstream>
#include <limits>
#include <stdexcept>

#include "server/wav_utils.hpp"

namespace piper_server {

void assembleChunkedWav(const ChunkedWavOutput &job) {
  std::uint64_t totalBytes = 0;
  for (const auto bytes : job.chunkBytes) {
    totalBytes += bytes;
  }

  if (totalBytes > std::numeric_limits<std::uint32_t>::max()) {
    throw std::runtime_error("Generated WAV exceeds 4 GiB. Use smaller inputs or raw output.");
  }

  std::ofstream output(job.outputPath, std::ios::binary);
  if (!output.good()) {
    throw std::runtime_error("Could not open output file");
  }

  writeServerWavHeader(job.sampleRate, job.sampleWidth, job.channels,
                       static_cast<std::uint32_t>(totalBytes), output);

  std::array<char, 64 * 1024> buffer{};
  for (const auto &chunkPath : job.chunkPaths) {
    std::ifstream chunk(chunkPath, std::ios::binary);
    if (!chunk.good()) {
      throw std::runtime_error("Missing synthesized chunk");
    }
    while (chunk.good()) {
      chunk.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
      const auto count = chunk.gcount();
      if (count > 0) {
        output.write(buffer.data(), count);
      }
    }
  }
}

} // namespace piper_server
