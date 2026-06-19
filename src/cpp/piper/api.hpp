#ifndef PIPER_API_H_
#define PIPER_API_H_

#include <cstddef>
#include <functional>
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

#include "types.hpp"
#include "core/text_chunker.hpp"

namespace piper {

bool isSingleCodepoint(std::string s);
Phoneme getCodepoint(std::string s);
std::string getVersion();

void initialize(PiperConfig &config);
void terminate(PiperConfig &config);

void loadVoice(PiperConfig &config, std::string modelPath,
               std::string modelConfigPath, Voice &voice,
               const std::optional<SpeakerId> &speakerId, bool useCuda,
               std::optional<int> cpuThreads = std::nullopt);

void textToAudio(PiperConfig &config, Voice &voice, std::string text,
                 std::vector<int16_t> &audioBuffer, SynthesisResult &result,
                 const std::function<void()> &audioCallback,
                 const std::function<bool()> &shouldCancel = nullptr);

void textToWavFile(PiperConfig &config, Voice &voice, std::string text,
                   std::ostream &audioFile, SynthesisResult &result,
                   std::size_t maxChunkBytes = 0,
                   const std::function<bool()> &shouldCancel = nullptr);

void textToWavFileFromStream(PiperConfig &config, Voice &voice,
                             std::istream &textStream,
                             std::ostream &audioFile, SynthesisResult &result,
                             std::size_t maxChunkBytes,
                             const std::function<bool()> &shouldCancel = nullptr);

} // namespace piper

#endif // PIPER_API_H_
