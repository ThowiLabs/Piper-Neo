#include "raw_audio_output.hpp"

#include <algorithm>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <vector>

namespace piper_app {

using namespace std;

void rawOutputProc(vector<int16_t> &sharedAudioBuffer, mutex &mutAudio,
                   condition_variable &cvAudio, bool &audioReady,
                   bool &audioFinished) {
  vector<int16_t> internalAudioBuffer;
  while (true) {
    {
      unique_lock lockAudio{mutAudio};
      cvAudio.wait(lockAudio, [&audioReady] { return audioReady; });

      if (sharedAudioBuffer.empty() && audioFinished) {
        break;
      }

      copy(sharedAudioBuffer.begin(), sharedAudioBuffer.end(),
           back_inserter(internalAudioBuffer));

      sharedAudioBuffer.clear();

      if (!audioFinished) {
        audioReady = false;
      }
    }

    cout.write((const char *)internalAudioBuffer.data(),
               sizeof(int16_t) * internalAudioBuffer.size());
    cout.flush();
    internalAudioBuffer.clear();
  }

} // rawOutputProc


} // namespace piper_app
