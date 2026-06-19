#ifndef PIPER_CORE_PIPELINE_PHONEMIZER_HPP_
#define PIPER_CORE_PIPELINE_PHONEMIZER_HPP_

#include "piper/types.hpp"

#include <vector>

namespace piper::core::pipeline {

std::vector<std::vector<Phoneme>> phonemizeTextForPipeline(
    const Voice &voice, const std::string &text);

} // namespace piper::core::pipeline

#endif // PIPER_CORE_PIPELINE_PHONEMIZER_HPP_
