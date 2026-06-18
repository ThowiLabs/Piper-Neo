#include "piper.hpp"

#include <string>

namespace piper {

#ifdef _PIPER_VERSION
#define _STR(x) #x
#define STR(x) _STR(x)
const std::string VERSION = STR(_PIPER_VERSION);
#else
const std::string VERSION = "";
#endif

std::string getVersion() { return VERSION; }

} // namespace piper
