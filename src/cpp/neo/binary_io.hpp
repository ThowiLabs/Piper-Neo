#ifndef PIPER_NEO_BINARY_IO_H_
#define PIPER_NEO_BINARY_IO_H_

#include <cstdint>
#include <iosfwd>
#include <string>

namespace piper_neo::detail {

std::uint32_t readU32(std::istream &in);
std::uint64_t readU64(std::istream &in);
std::string readString(std::istream &in);

void writeU32(std::ostream &out, std::uint32_t value);
void writeU64(std::ostream &out, std::uint64_t value);
void writeString(std::ostream &out, const std::string &value);

} // namespace piper_neo::detail

#endif // PIPER_NEO_BINARY_IO_H_
