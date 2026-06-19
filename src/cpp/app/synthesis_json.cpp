#include "synthesis_json.hpp"

#include <spdlog/spdlog.h>

namespace piper_app {

void applyJsonLineOverrides(const nlohmann::json &lineRoot, piper::Voice &voice,
                            OutputType &outputType,
                            std::optional<std::filesystem::path> &maybeOutputPath) {
  if (lineRoot.contains("output_file")) {
    outputType = OUTPUT_FILE;
    maybeOutputPath = std::filesystem::path(lineRoot["output_file"].get<std::string>());
  }

  if (lineRoot.contains("speaker_id")) {
    voice.synthesisConfig.speakerId = lineRoot["speaker_id"].get<piper::SpeakerId>();
    return;
  }

  if (!lineRoot.contains("speaker")) {
    return;
  }

  auto speakerName = lineRoot["speaker"].get<std::string>();
  if ((voice.modelConfig.speakerIdMap) &&
      (voice.modelConfig.speakerIdMap->count(speakerName) > 0)) {
    voice.synthesisConfig.speakerId = (*voice.modelConfig.speakerIdMap)[speakerName];
  } else {
    spdlog::warn("No speaker named: {}", speakerName);
  }
}

} // namespace piper_app
