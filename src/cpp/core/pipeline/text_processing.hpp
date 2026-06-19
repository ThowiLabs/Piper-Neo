#ifndef PIPER_CORE_PIPELINE_TEXT_PROCESSING_HPP_
#define PIPER_CORE_PIPELINE_TEXT_PROCESSING_HPP_

#include "piper/types.hpp"

#include <string>

namespace piper::core::pipeline {

std::string normalizeTextForPipeline(const Voice &voice, const std::string &text,
                                     bool allowTextNormalization);

std::string diacritizeTextForPipeline(PiperConfig &config, std::string text);

} // namespace piper::core::pipeline

#endif // PIPER_CORE_PIPELINE_TEXT_PROCESSING_HPP_
