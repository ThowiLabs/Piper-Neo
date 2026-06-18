#include "output_cleanup.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <thread>

#include <spdlog/spdlog.h>

#include "utils.hpp"

namespace piper_server {

bool isManagedWavFile(const std::filesystem::directory_entry &entry) {
  if (!entry.is_regular_file()) {
    return false;
  }
  return lowerCopy(entry.path().extension().string()) == ".wav";
}

void cleanupTempDirectory(const ServerOptions &options) {
  std::error_code ignored;
  const auto tempDir = options.outputDir / "tmp";
  if (std::filesystem::exists(tempDir, ignored)) {
    std::filesystem::remove_all(tempDir, ignored);
    spdlog::info("{} Temp cleanup completed: dir={}", nowIso8601(), tempDir.filename().string());
  }
  std::filesystem::create_directories(tempDir, ignored);
}

void cleanupExpiredOutputFiles(const ServerOptions &options) {
  if (options.outputRetentionSeconds == 0) {
    return;
  }

  std::error_code ignored;
  if (!std::filesystem::exists(options.outputDir, ignored)) {
    return;
  }

  const auto cutoff = std::filesystem::file_time_type::clock::now() -
                      std::chrono::seconds(options.outputRetentionSeconds);
  std::size_t removed = 0;
  for (const auto &entry : std::filesystem::directory_iterator(options.outputDir, ignored)) {
    if (ignored || !isManagedWavFile(entry)) {
      continue;
    }

    const auto modified = entry.last_write_time(ignored);
    if (ignored) {
      ignored.clear();
      continue;
    }

    if (modified < cutoff) {
      std::filesystem::remove(entry.path(), ignored);
      if (!ignored) {
        ++removed;
      } else {
        ignored.clear();
      }
    }
  }

  if (removed > 0) {
    spdlog::info("{} Output retention cleanup: removed={} retention_seconds={}",
                 nowIso8601(), removed, options.outputRetentionSeconds);
  }
}

void startOutputCleanupThread(ServerOptions options) {
  std::thread([options]() {
    const auto sleepSeconds = std::max<std::size_t>(30,
        std::min<std::size_t>(300, options.outputRetentionSeconds == 0 ? 300 : options.outputRetentionSeconds / 4));
    while (true) {
      std::this_thread::sleep_for(std::chrono::seconds(sleepSeconds));
      cleanupExpiredOutputFiles(options);
    }
  }).detach();
}

} // namespace piper_server
