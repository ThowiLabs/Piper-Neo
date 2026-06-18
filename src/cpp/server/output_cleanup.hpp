#ifndef PIPER_SERVER_OUTPUT_CLEANUP_H_
#define PIPER_SERVER_OUTPUT_CLEANUP_H_

#include "../server.hpp"

namespace piper_server {

void cleanupTempDirectory(const ServerOptions &options);
void cleanupExpiredOutputFiles(const ServerOptions &options);
void startOutputCleanupThread(ServerOptions options);

} // namespace piper_server

#endif // PIPER_SERVER_OUTPUT_CLEANUP_H_
