#ifndef PIPER_APP_RAW_AUDIO_OUTPUT_HPP_
#define PIPER_APP_RAW_AUDIO_OUTPUT_HPP_

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <vector>

namespace piper_app {

void rawOutputProc(std::vector<int16_t> &sharedAudioBuffer, std::mutex &mutAudio,
                   std::condition_variable &cvAudio, bool &audioReady,
                   bool &audioFinished);

} // namespace piper_app

#endif // PIPER_APP_RAW_AUDIO_OUTPUT_HPP_
