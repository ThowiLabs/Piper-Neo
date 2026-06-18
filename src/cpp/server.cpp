#include "server.hpp"

#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include <spdlog/spdlog.h>

#include "server/http.hpp"
#include "server/model_cache.hpp"
#include "server/model_registry.hpp"
#include "server/output_cleanup.hpp"
#include "server/request_handler.hpp"
#include "server/tts_scheduler.hpp"

namespace piper_server {

void runServer(piper::PiperConfig &piperConfig, piper::Voice &voice,
               ServerOptions options) {
  std::filesystem::create_directories(options.modelsDir);
  std::filesystem::create_directories(options.outputDir);
  cleanupTempDirectory(options);
  cleanupExpiredOutputFiles(options);
  startOutputCleanupThread(options);

#ifdef _WIN32
  WSADATA wsaData;
  if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
    throw std::runtime_error("WSAStartup failed");
  }
#endif

  SocketHandle serverSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (serverSocket == INVALID_SOCKET_HANDLE) {
    throw std::runtime_error("Could not create server socket");
  }

  int reuse = 1;
#ifdef _WIN32
  setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR,
             reinterpret_cast<const char *>(&reuse), sizeof(reuse));
#else
  setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#endif

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(static_cast<uint16_t>(options.port));
  if (inet_pton(AF_INET, options.host.c_str(), &address.sin_addr) <= 0) {
    closeSocket(serverSocket);
    throw std::runtime_error("Invalid --host value. Use an IPv4 address like 127.0.0.1 or 0.0.0.0");
  }

  if (bind(serverSocket, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0) {
    closeSocket(serverSocket);
    throw std::runtime_error("Could not bind server socket");
  }

  if (listen(serverSocket, 16) < 0) {
    closeSocket(serverSocket);
    throw std::runtime_error("Could not listen on server socket");
  }

  unsigned int hardwareThreads = options.detectedHardwareThreads;
  if (hardwareThreads == 0) {
    hardwareThreads = std::thread::hardware_concurrency();
  }
  if (hardwareThreads == 0) {
    hardwareThreads = 1;
  }
  options.resourcePolicy.profile = options.cpuProfile;
  options.resourcePolicy.autoConfigured = true;
  options.resourcePolicy.hardwareThreads = hardwareThreads;
  options.resourcePolicy.memoryBytes = options.detectedMemoryBytes;
  options.resourcePolicy.cpuThreadsPerWorker = static_cast<std::size_t>(options.cpuThreads.value_or(1));
  options.resourcePolicy.maxConcurrentJobs = options.maxConcurrentJobs;
  options.resourcePolicy.chunkWorkers = options.chunkWorkers;
  options.resourcePolicy.maxModelReplicas = options.maxModelReplicas;
  options.resourcePolicy.queueSize = options.queueSize;
  options.resourcePolicy.queueTimeoutSeconds = options.queueTimeoutSeconds;
  options.resourcePolicy.maxTempBytes = options.maxTempBytes;

  spdlog::info("Piper API server listening on http://{}:{}", options.host,
               options.port);
  spdlog::info("Models directory: {}", options.modelsDir.string());
  spdlog::info("Active model: {}", options.activeModelPath.string());
  spdlog::info("Output directory: {}", options.outputDir.string());
  if (options.apiToken.empty()) {
    spdlog::warn("API token is not configured; HTTP API is open on this bind address");
  } else {
    spdlog::info("API token authentication enabled");
  }
  spdlog::info("Resource policy: profile={} detected_threads={} detected_memory_mb={} cpu_threads={} chunk_workers={} max_jobs={} queue_size={} replicas={} max_temp_bytes={}",
               options.cpuProfile, hardwareThreads,
               options.detectedMemoryBytes == 0 ? 0 : options.detectedMemoryBytes / (1024ULL * 1024ULL),
               options.cpuThreads.value_or(0), options.chunkWorkers,
               options.maxConcurrentJobs, options.queueSize, options.maxModelReplicas,
               options.maxTempBytes);

  ModelRegistry modelRegistry(options);
  modelRegistry.forceRefresh();
  ModelCache modelCache(piperConfig, voice, options, modelRegistry);
  ServerMetrics metrics;
  FairTtsScheduler scheduler(piperConfig, modelCache, options, metrics);

  while (true) {
    sockaddr_in clientAddress{};
#ifdef _WIN32
    int clientLength = sizeof(clientAddress);
#else
    socklen_t clientLength = sizeof(clientAddress);
#endif
    SocketHandle clientSocket =
        accept(serverSocket, reinterpret_cast<sockaddr *>(&clientAddress), &clientLength);
    if (clientSocket == INVALID_SOCKET_HANDLE) {
      continue;
    }

    std::thread(handleClient, clientSocket, options,
                std::ref(modelCache), std::ref(modelRegistry),
                std::ref(scheduler), std::ref(metrics))
        .detach();
  }

  closeSocket(serverSocket);
#ifdef _WIN32
  WSACleanup();
#endif
}


} // namespace piper_server
