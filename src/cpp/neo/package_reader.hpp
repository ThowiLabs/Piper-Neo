#ifndef PIPER_NEO_PACKAGE_READER_H_
#define PIPER_NEO_PACKAGE_READER_H_

#include <filesystem>
#include <string>
#include <vector>

#include "neo/package_types.hpp"

namespace piper_neo::detail {

ParsedPackage parsePackage(const std::filesystem::path &path);
const SectionEntry *findSection(const ParsedPackage &package, const std::string &name);
std::vector<char> readSectionBytes(const ParsedPackage &package, const SectionEntry &entry);
std::string compressionName(std::uint32_t compression);

} // namespace piper_neo::detail

#endif // PIPER_NEO_PACKAGE_READER_H_
