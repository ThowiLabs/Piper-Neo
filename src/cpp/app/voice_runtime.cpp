#include "voice_runtime.hpp"

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <sstream>

#include <spdlog/spdlog.h>

#include "platform.hpp"

namespace piper_app {

namespace {

std::string utcTimestampNow() {
  const auto now = std::chrono::system_clock::now();
  const auto time = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
#ifdef _WIN32
  gmtime_s(&tm, &time);
#else
  gmtime_r(&time, &tm);
#endif

  std::ostringstream out;
  out << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
  return out.str();
}

void configurePhonemizerRuntime(const RunConfig &runConfig, const char *argv0,
                                const piper::Voice &voice,
                                piper::PiperConfig &piperConfig) {
  const auto exePath = resolveExecutablePath(argv0);

  if (voice.phonemizeConfig.phonemeType == piper::eSpeakPhonemes) {
    spdlog::debug("Voice uses eSpeak phonemes ({})",
                  voice.phonemizeConfig.eSpeak.voice);

    if (runConfig.eSpeakDataPath) {
      piperConfig.eSpeakDataPath = runConfig.eSpeakDataPath.value().string();
    } else {
      piperConfig.eSpeakDataPath =
          std::filesystem::absolute(exePath.parent_path().append("espeak-ng-data")).string();
      spdlog::debug("espeak-ng-data directory is expected at {}",
                    piperConfig.eSpeakDataPath);
    }
  } else {
    piperConfig.useESpeak = false;
  }

  if (voice.phonemizeConfig.eSpeak.voice == "ar") {
    piperConfig.useTashkeel = true;
    if (runConfig.tashkeelModelPath) {
      piperConfig.tashkeelModelPath = runConfig.tashkeelModelPath.value().string();
    } else {
      piperConfig.tashkeelModelPath =
          std::filesystem::absolute(exePath.parent_path().append("libtashkeel_model.ort")).string();
      spdlog::debug("libtashkeel model is expected at {}",
                    piperConfig.tashkeelModelPath.value());
    }
  }
}

} // namespace

void prepareVoiceRuntime(const RunConfig &runConfig, const char *argv0,
                         piper::PiperConfig &piperConfig, piper::Voice &voice,
                         std::optional<piper_neo::ExtractedNeoModel> &extractedNeoModel) {
  std::filesystem::path loadModelPath = runConfig.modelPath;
  std::filesystem::path loadConfigPath = runConfig.modelConfigPath;

  if (piper_neo::isNeoFile(runConfig.modelPath)) {
    extractedNeoModel = piper_neo::extractPackage(
        runConfig.modelPath,
        std::filesystem::temp_directory_path() / "piper-neo-cli-cache");
    loadModelPath = extractedNeoModel->modelPath;
    loadConfigPath = extractedNeoModel->configPath;
  }

  spdlog::debug("Loading voice from {} (config={})", loadModelPath.string(),
                loadConfigPath.string());

  const auto timestamp = utcTimestampNow();
  const auto format = piper_neo::isNeoFile(runConfig.modelPath) ? "neo" : "onnx";
  spdlog::info("{} Model load started: model={} format={}", timestamp,
               runConfig.modelPath.filename().string(), format);

  const auto start = std::chrono::steady_clock::now();
  loadVoice(piperConfig, loadModelPath.string(), loadConfigPath.string(), voice,
            runConfig.speakerId, runConfig.useCuda, runConfig.cpuThreads);
  const auto end = std::chrono::steady_clock::now();

  spdlog::info("{} Model load finished: model={} format={} duration_ms={}", timestamp,
               runConfig.modelPath.filename().string(), format,
               std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());

  configurePhonemizerRuntime(runConfig, argv0, voice, piperConfig);
  piper::initialize(piperConfig);
}

void applySynthesisOverrides(const RunConfig &runConfig, piper::Voice &voice) {
  if (runConfig.noiseScale) {
    voice.synthesisConfig.noiseScale = runConfig.noiseScale.value();
  }

  if (runConfig.lengthScale) {
    voice.synthesisConfig.lengthScale = runConfig.lengthScale.value();
  }

  if (runConfig.noiseW) {
    voice.synthesisConfig.noiseW = runConfig.noiseW.value();
  }

  if (runConfig.sentenceSilenceSeconds) {
    voice.synthesisConfig.sentenceSilenceSeconds =
        runConfig.sentenceSilenceSeconds.value();
  }

  if (!runConfig.phonemeSilenceSeconds) {
    return;
  }

  if (!voice.synthesisConfig.phonemeSilenceSeconds) {
    voice.synthesisConfig.phonemeSilenceSeconds = runConfig.phonemeSilenceSeconds;
    return;
  }

  for (const auto &[phoneme, silenceSeconds] : *runConfig.phonemeSilenceSeconds) {
    voice.synthesisConfig.phonemeSilenceSeconds->try_emplace(phoneme, silenceSeconds);
  }
}

} // namespace piper_app
