#include "server/routes/tts_payload.hpp"

namespace piper_server {

json ttsSuccessPayload(const std::string &fileName, const TtsJobResult &result,
                       const json &markup, const json &textPreprocessing,
                       const std::optional<std::string> &fallbackModel) {
  return json{{"file", fileName},
              {"model", fallbackModel.value_or(result.modelName)},
              {"url", "/api/v1/files/" + fileName},
              {"format", "wav"},
              {"chunks", result.chunks},
              {"bytes", result.bytes},
              {"audio_seconds", result.synthesis.audioSeconds},
              {"infer_seconds", result.synthesis.inferSeconds},
              {"real_time_factor", result.synthesis.realTimeFactor},
              {"markup", markup},
              {"text_preprocessing", textPreprocessing}};
}

} // namespace piper_server
