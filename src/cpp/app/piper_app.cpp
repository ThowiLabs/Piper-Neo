#include "piper_app.hpp"

#include <cstdlib>
#include <optional>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "cli_args.hpp"
#include "export_neo_mode.hpp"
#include "hardware.hpp"
#include "platform.hpp"
#include "run_config.hpp"
#include "server_mode.hpp"
#include "synthesis_mode.hpp"
#include "voice_runtime.hpp"
#include "../neo_model.hpp"
#include "../piper.hpp"

namespace piper_app {

namespace {

class PiperRuntimeGuard {
public:
  explicit PiperRuntimeGuard(piper::PiperConfig &config) : config_(config) {}
  ~PiperRuntimeGuard() { piper::terminate(config_); }

  PiperRuntimeGuard(const PiperRuntimeGuard &) = delete;
  PiperRuntimeGuard &operator=(const PiperRuntimeGuard &) = delete;

private:
  piper::PiperConfig &config_;
};

} // namespace

int piperMain(int argc, char *argv[]) {
  spdlog::set_default_logger(spdlog::stderr_color_st("piper"));

  RunConfig runConfig;
  parseArgs(argc, argv, runConfig);

  if (runConfig.exportNeoPath) {
    return runExportNeoMode(runConfig);
  }

  configureConsoleUtf8();

  piper::PiperConfig piperConfig;
  piper::Voice voice;
  std::optional<piper_neo::ExtractedNeoModel> extractedNeoModel;

  prepareVoiceRuntime(runConfig, argv[0], piperConfig, voice, extractedNeoModel);
  PiperRuntimeGuard runtimeGuard(piperConfig);
  applySynthesisOverrides(runConfig, voice);

  if (runConfig.serverMode) {
    return runServerMode(piperConfig, voice, runConfig);
  }

  return runSynthesisMode(piperConfig, voice, runConfig);
}

} // namespace piper_app
