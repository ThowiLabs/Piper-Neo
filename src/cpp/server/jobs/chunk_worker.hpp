#ifndef PIPER_SERVER_JOBS_CHUNK_WORKER_H_
#define PIPER_SERVER_JOBS_CHUNK_WORKER_H_

#include <cstddef>
#include <memory>

#include "server/jobs/job_state.hpp"
#include "server/model_cache.hpp"
#include "server.hpp"

namespace piper_server {

void synthesizeJobChunk(piper::PiperConfig &piperConfig, ModelCache &modelCache,
                        const ServerOptions &options, ServerMetrics &metrics,
                        const std::shared_ptr<TtsJobState> &job,
                        std::size_t index);

} // namespace piper_server

#endif // PIPER_SERVER_JOBS_CHUNK_WORKER_H_
