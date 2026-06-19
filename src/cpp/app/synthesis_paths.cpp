#include "synthesis_paths.hpp"

#include <chrono>
#include <sstream>

namespace piper_app {

std::filesystem::path timestampedOutputPath(const RunConfig &runConfig) {
  const auto now = std::chrono::system_clock::now();
  const auto timestamp =
      std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
  std::stringstream outputName;
  outputName << timestamp << ".wav";

  std::filesystem::path outputPath = runConfig.outputPath.value_or(".");
  outputPath.append(outputName.str());
  return outputPath;
}

} // namespace piper_app
