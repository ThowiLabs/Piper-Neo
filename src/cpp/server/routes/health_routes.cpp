#include "server/routes/health_routes.hpp"

#include <algorithm>

#include "server/metrics_report.hpp"
#include "server/responses.hpp"
#include "server/utils.hpp"

namespace piper_server {

bool handleHealthRoute(SocketHandle clientSocket, const HttpRequest &request,
                       const ParsedTarget &target, const RouteContext &context) {
  if (request.method != "GET") {
    return false;
  }

  if (target.path == "/api/health") {
    sendJson(clientSocket, 200,
             successResponse("Servidor Piper activo.",
                             json{{"status", "ok"},
                                  {"model_loaded", true},
                                  {"time", nowIso8601()}}));
    return true;
  }

  if (target.path == "/api/v1/status") {
    const auto &options = context.options;
    sendJson(clientSocket, 200,
             successResponse("Estado obtenido correctamente.",
                             json{{"server", "piper-neo"},
                                  {"model_loaded", true},
                                  {"active_model", options.activeModelPath.filename().string()},
                                  {"models_dir", options.modelsDir.filename().string()},
                                  {"output_dir", options.outputDir.filename().string()},
                                  {"auth",
                                   json{{"enabled", !options.apiToken.empty()},
                                        {"header", "Authorization: Bearer <token>"}}},
                                  {"limits",
                                   json{{"max_input_bytes", options.maxInputBytes},
                                        {"max_text_chunk_bytes", options.maxTextChunkBytes},
                                        {"text_sanitizer_enabled", true},
                                        {"max_sanitized_text_chars",
                                         std::max<std::size_t>(
                                             1000, std::min<std::size_t>(50000, options.maxInputBytes))},
                                        {"max_temp_bytes", options.maxTempBytes},
                                        {"output_retention_seconds", options.outputRetentionSeconds},
                                        {"models_refresh_seconds", options.modelsRefreshSeconds}}},
                                  {"resource_policy", resourcePolicyJson(options, context.scheduler)}}));
    return true;
  }

  if (target.path == "/api/v1/metrics") {
    sendJson(clientSocket, 200,
             successResponse("Métricas obtenidas correctamente.",
                             metricsJson(context.metrics, context.scheduler, context.options)));
    return true;
  }

  return false;
}

} // namespace piper_server
