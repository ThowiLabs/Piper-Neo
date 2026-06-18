#ifndef PIPER_APP_CLI_ARGS_HPP_
#define PIPER_APP_CLI_ARGS_HPP_

#include "run_config.hpp"

namespace piper_app {

void printUsage(char *argv[]);
void parseArgs(int argc, char *argv[], RunConfig &runConfig);

} // namespace piper_app

#endif // PIPER_APP_CLI_ARGS_HPP_
