#ifndef PIPER_NEO_PACKAGE_WRITER_H_
#define PIPER_NEO_PACKAGE_WRITER_H_

#include <filesystem>
#include <optional>

namespace piper_neo::detail {

void writePackageFromOnnxImpl(const std::filesystem::path &onnxPath,
                              const std::filesystem::path &configPath,
                              const std::filesystem::path &outputNeoPath,
                              const std::optional<std::filesystem::path> &imagePath,
                              int compressionLevel);

} // namespace piper_neo::detail

#endif // PIPER_NEO_PACKAGE_WRITER_H_
