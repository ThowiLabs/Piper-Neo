#include "export_neo_mode.hpp"

#include <chrono>
#include <cstdlib>

#include <spdlog/spdlog.h>

#include "../neo_model.hpp"

namespace piper_app {

int runExportNeoMode(const RunConfig &runConfig) {
  const auto start = std::chrono::steady_clock::now();
  spdlog::info("Exporting Piper Neo package: output={} compression=zstd level={}",
               runConfig.exportNeoPath->string(), runConfig.neoCompressionLevel);

  piper_neo::writePackageFromOnnx(runConfig.modelPath, runConfig.modelConfigPath,
                                  *runConfig.exportNeoPath, runConfig.neoImagePath,
                                  runConfig.neoCompressionLevel);

  const auto end = std::chrono::steady_clock::now();
  spdlog::info("Piper Neo package exported in {} ms",
               std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
  return EXIT_SUCCESS;
}

} // namespace piper_app
