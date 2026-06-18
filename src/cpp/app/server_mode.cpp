#include "server_mode.hpp"

#include <cstdlib>
#include <filesystem>

#include "env.hpp"
#include "../server.hpp"

namespace piper_app {

int runServerMode(piper::PiperConfig &piperConfig, piper::Voice &voice,
                  const RunConfig &runConfig) {
  piper_server::ServerOptions serverOptions;
  serverOptions.host = runConfig.serverHost;
  serverOptions.port = runConfig.serverPort;
  serverOptions.modelsDir = std::filesystem::absolute(runConfig.modelsDir);
  serverOptions.outputDir = std::filesystem::absolute(
      runConfig.outputDirExplicit ? runConfig.outputPath.value()
                                  : std::filesystem::path("outputs"));
  serverOptions.activeModelPath = std::filesystem::absolute(runConfig.modelPath);
  serverOptions.activeModelConfigPath = runConfig.modelConfigPath.empty()
                                            ? std::filesystem::path()
                                            : std::filesystem::absolute(runConfig.modelConfigPath);
  serverOptions.defaultSpeakerId = runConfig.speakerId;
  serverOptions.useCuda = runConfig.useCuda;
  serverOptions.cpuThreads = runConfig.cpuThreads;
  serverOptions.maxInputBytes = runConfig.maxInputBytes;
  serverOptions.maxTextChunkBytes = runConfig.maxTextChunkBytes;
  serverOptions.maxConcurrentJobs = runConfig.maxConcurrentJobs;
  serverOptions.maxModelReplicas = runConfig.maxModelReplicas;
  serverOptions.chunkWorkers = runConfig.chunkWorkers;
  serverOptions.queueSize = runConfig.queueSize;
  serverOptions.queueTimeoutSeconds = runConfig.queueTimeoutSeconds;
  serverOptions.maxTempBytes = runConfig.maxTempBytes;
  serverOptions.outputRetentionSeconds = runConfig.outputRetentionSeconds;
  serverOptions.modelsRefreshSeconds = runConfig.modelsRefreshSeconds;
  serverOptions.cpuProfile = runConfig.cpuProfile;
  serverOptions.detectedHardwareThreads = runConfig.detectedHardwareThreads;
  serverOptions.detectedMemoryBytes = runConfig.detectedMemoryBytes;
  serverOptions.apiToken = resolveApiToken(runConfig.apiToken).value_or("");

  piper_server::runServer(piperConfig, voice, serverOptions);
  return EXIT_SUCCESS;
}

} // namespace piper_app
