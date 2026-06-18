#ifndef PIPER_CORE_MODEL_RUNTIME_HPP_
#define PIPER_CORE_MODEL_RUNTIME_HPP_

#include "piper/types.hpp"

#include <optional>
#include <string>
#include <vector>

namespace piper::core {

void loadModel(const std::string &modelPath, ModelSession &session, bool useCuda,
               std::optional<int> cpuThreads);

void synthesizePhonemeIds(std::vector<PhonemeId> &phonemeIds,
                          SynthesisConfig &synthesisConfig,
                          ModelSession &session,
                          std::vector<int16_t> &audioBuffer,
                          SynthesisResult &result);

} // namespace piper::core

#endif // PIPER_CORE_MODEL_RUNTIME_HPP_
