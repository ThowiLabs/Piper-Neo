#ifndef PIPER_SERVER_JOBS_JOB_LIFECYCLE_H_
#define PIPER_SERVER_JOBS_JOB_LIFECYCLE_H_

#include <memory>

#include "server/jobs/job_state.hpp"
#include "server.hpp"

namespace piper_server {

std::shared_ptr<TtsJobState> createJobState(const TtsJobRequest &request,
                                            const ServerOptions &options);
void markChunkFinished(TtsJobState &job);
void assembleJobWav(const TtsJobState &job);
void cleanupJobTemp(TtsJobState &job, ServerMetrics &metrics);
TtsJobResult makeJobResult(const TtsJobState &job);

} // namespace piper_server

#endif // PIPER_SERVER_JOBS_JOB_LIFECYCLE_H_
