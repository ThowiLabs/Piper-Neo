#include "neo/binary_io.hpp"

#include <istream>
#include <ostream>
#include <stdexcept>

namespace piper_neo::detail {

std::uint32_t readU32(std::istream &in) {
  unsigned char b[4]{};
  in.read(reinterpret_cast<char *>(b), 4);
  if (!in.good()) {
    throw std::runtime_error("invalid_neo");
  }
  return static_cast<std::uint32_t>(b[0]) |
         (static_cast<std::uint32_t>(b[1]) << 8) |
         (static_cast<std::uint32_t>(b[2]) << 16) |
         (static_cast<std::uint32_t>(b[3]) << 24);
}

std::uint64_t readU64(std::istream &in) {
  unsigned char b[8]{};
  in.read(reinterpret_cast<char *>(b), 8);
  if (!in.good()) {
    throw std::runtime_error("invalid_neo");
  }

  std::uint64_t value = 0;
  for (int i = 7; i >= 0; --i) {
    value = (value << 8) | b[i];
  }
  return value;
}

std::string readString(std::istream &in) {
  auto size = readU32(in);
  if (size > 64 * 1024 * 1024) {
    throw std::runtime_error("invalid_neo");
  }

  std::string value(size, '\0');
  if (size > 0) {
    in.read(value.data(), static_cast<std::streamsize>(size));
    if (!in.good()) {
      throw std::runtime_error("invalid_neo");
    }
  }
  return value;
}

void writeU32(std::ostream &out, std::uint32_t value) {
  unsigned char b[4]{static_cast<unsigned char>(value & 0xff),
                     static_cast<unsigned char>((value >> 8) & 0xff),
                     static_cast<unsigned char>((value >> 16) & 0xff),
                     static_cast<unsigned char>((value >> 24) & 0xff)};
  out.write(reinterpret_cast<const char *>(b), 4);
}

void writeU64(std::ostream &out, std::uint64_t value) {
  unsigned char b[8]{};
  for (int i = 0; i < 8; ++i) {
    b[i] = static_cast<unsigned char>((value >> (8 * i)) & 0xff);
  }
  out.write(reinterpret_cast<const char *>(b), 8);
}

void writeString(std::ostream &out, const std::string &value) {
  writeU32(out, static_cast<std::uint32_t>(value.size()));
  out.write(value.data(), static_cast<std::streamsize>(value.size()));
}

} // namespace piper_neo::detail
