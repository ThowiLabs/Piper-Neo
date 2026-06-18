#include "core/model_runtime.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <limits>
#include <stdexcept>

#include <onnxruntime_cxx_api.h>
#include <spdlog/spdlog.h>

namespace piper::core {
namespace {

constexpr float MAX_WAV_VALUE = 32767.0f;
const std::string instanceName{"piper"};

#ifdef _WIN32
std::wstring toWidePath(const std::string &path) {
  // Preserve existing behavior: Piper paths are expected to be ASCII/UTF-8 safe
  // in the current Windows build flow. A future Windows-only hardening pass can
  // replace this with MultiByteToWideChar for full Unicode paths.
  return std::wstring(path.begin(), path.end());
}
#endif

} // namespace

void loadModel(const std::string &modelPath, ModelSession &session, bool useCuda,
               std::optional<int> cpuThreads) {
  spdlog::debug("Loading onnx model from {}", modelPath);
  session.env = Ort::Env(OrtLoggingLevel::ORT_LOGGING_LEVEL_WARNING,
                         instanceName.c_str());
  session.env.DisableTelemetryEvents();

  if (useCuda) {
    OrtCUDAProviderOptions cudaOptions{};
    cudaOptions.cudnn_conv_algo_search = OrtCudnnConvAlgoSearchHeuristic;
    session.options.AppendExecutionProvider_CUDA(cudaOptions);
  }

  if (cpuThreads && (cpuThreads.value() > 0)) {
    session.options.SetIntraOpNumThreads(cpuThreads.value());
    session.options.SetInterOpNumThreads(1);
    spdlog::info("ONNX Runtime CPU threads limited to {}", cpuThreads.value());
  }

  session.options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_DISABLE_ALL);
  session.options.DisableCpuMemArena();
  session.options.DisableMemPattern();
  session.options.DisableProfiling();

  const auto startTime = std::chrono::steady_clock::now();

#ifdef _WIN32
  const auto modelPathW = toWidePath(modelPath);
  const auto *modelPathStr = modelPathW.c_str();
#else
  const auto *modelPathStr = modelPath.c_str();
#endif

  session.onnx = Ort::Session(session.env, modelPathStr, session.options);

  const auto endTime = std::chrono::steady_clock::now();
  spdlog::debug("Loaded onnx model in {} second(s)",
                std::chrono::duration<double>(endTime - startTime).count());
}

void synthesizePhonemeIds(std::vector<PhonemeId> &phonemeIds,
                          SynthesisConfig &synthesisConfig,
                          ModelSession &session,
                          std::vector<int16_t> &audioBuffer,
                          SynthesisResult &result) {
  spdlog::debug("Synthesizing audio for {} phoneme id(s)", phonemeIds.size());

  auto memoryInfo = Ort::MemoryInfo::CreateCpu(
      OrtAllocatorType::OrtArenaAllocator, OrtMemType::OrtMemTypeDefault);

  std::vector<int64_t> phonemeIdLengths{static_cast<int64_t>(phonemeIds.size())};
  std::vector<float> scales{synthesisConfig.noiseScale,
                            synthesisConfig.lengthScale,
                            synthesisConfig.noiseW};

  std::vector<Ort::Value> inputTensors;
  std::vector<int64_t> phonemeIdsShape{1, static_cast<int64_t>(phonemeIds.size())};
  inputTensors.push_back(Ort::Value::CreateTensor<int64_t>(
      memoryInfo, phonemeIds.data(), phonemeIds.size(), phonemeIdsShape.data(),
      phonemeIdsShape.size()));

  std::vector<int64_t> phonemeIdLengthsShape{
      static_cast<int64_t>(phonemeIdLengths.size())};
  inputTensors.push_back(Ort::Value::CreateTensor<int64_t>(
      memoryInfo, phonemeIdLengths.data(), phonemeIdLengths.size(),
      phonemeIdLengthsShape.data(), phonemeIdLengthsShape.size()));

  std::vector<int64_t> scalesShape{static_cast<int64_t>(scales.size())};
  inputTensors.push_back(Ort::Value::CreateTensor<float>(
      memoryInfo, scales.data(), scales.size(), scalesShape.data(),
      scalesShape.size()));

  std::vector<int64_t> speakerId{
      static_cast<int64_t>(synthesisConfig.speakerId.value_or(0))};
  std::vector<int64_t> speakerIdShape{static_cast<int64_t>(speakerId.size())};

  if (synthesisConfig.speakerId) {
    inputTensors.push_back(Ort::Value::CreateTensor<int64_t>(
        memoryInfo, speakerId.data(), speakerId.size(), speakerIdShape.data(),
        speakerIdShape.size()));
  }

  std::array<const char *, 4> inputNames = {"input", "input_lengths", "scales",
                                            "sid"};
  std::array<const char *, 1> outputNames = {"output"};

  const auto startTime = std::chrono::steady_clock::now();
  auto outputTensors = session.onnx.Run(
      Ort::RunOptions{nullptr}, inputNames.data(), inputTensors.data(),
      inputTensors.size(), outputNames.data(), outputNames.size());
  const auto endTime = std::chrono::steady_clock::now();

  if ((outputTensors.size() != 1) || (!outputTensors.front().IsTensor())) {
    throw std::runtime_error("Invalid output tensors");
  }

  const auto inferDuration = std::chrono::duration<double>(endTime - startTime);
  result.inferSeconds = inferDuration.count();

  const float *audio = outputTensors.front().GetTensorData<float>();
  const auto audioShape =
      outputTensors.front().GetTensorTypeAndShapeInfo().GetShape();
  const int64_t audioCount = audioShape[audioShape.size() - 1];

  result.audioSeconds = static_cast<double>(audioCount) /
                        static_cast<double>(synthesisConfig.sampleRate);
  result.realTimeFactor = 0.0;
  if (result.audioSeconds > 0) {
    result.realTimeFactor = result.inferSeconds / result.audioSeconds;
  }

  spdlog::debug("Synthesized {} second(s) of audio in {} second(s)",
                result.audioSeconds, result.inferSeconds);

  float maxAudioValue = 0.01f;
  for (int64_t i = 0; i < audioCount; i++) {
    const float audioValue = std::abs(audio[i]);
    if (audioValue > maxAudioValue) {
      maxAudioValue = audioValue;
    }
  }

  audioBuffer.reserve(audioBuffer.size() + audioCount);

  const float audioScale = MAX_WAV_VALUE / std::max(0.01f, maxAudioValue);
  for (int64_t i = 0; i < audioCount; i++) {
    const auto intAudioValue = static_cast<int16_t>(std::clamp(
        audio[i] * audioScale,
        static_cast<float>(std::numeric_limits<int16_t>::min()),
        static_cast<float>(std::numeric_limits<int16_t>::max())));

    audioBuffer.push_back(intAudioValue);
  }

  // Ort::Value owns its native handles and releases them through RAII.
  // Keeping ownership here is safer than manually releasing every handle because
  // exceptions during synthesis still clean up tensors correctly.
}

} // namespace piper::core
