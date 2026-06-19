#include "server/routes/tts_request.hpp"

#include <cassert>
#include <iostream>
#include <string>

namespace {

piper_server::HttpRequest postJson(const std::string &body) {
  piper_server::HttpRequest request;
  request.method = "POST";
  request.path = "/api/v1/tts";
  request.body = body;
  return request;
}

} // namespace

int main() {
  using namespace piper_server;

  {
    auto parsed = parseTtsRouteRequest(
        postJson(R"({"text":"Hola","model":"voz.onnx","speaker_id":2,"noise_scale":0.5,"lengthScale":1.2,"noiseW":0.7,"sentence_silence_seconds":0.2})"),
        1024);
    assert(parsed.ok);
    assert(parsed.value.text == "Hola");
    assert(parsed.value.requestedModel.value() == "voz.onnx");
    assert(parsed.value.speakerId.value() == 2);
    assert(parsed.value.noiseScale.value() == 0.5f);
    assert(parsed.value.lengthScale.value() == 1.2f);
    assert(parsed.value.noiseW.value() == 0.7f);
    assert(parsed.value.sentenceSilenceSeconds.value() == 0.2f);
  }

  {
    auto parsed = parseTtsRouteRequest(postJson("{no-json"), 1024);
    assert(!parsed.ok);
    assert(parsed.status == 400);
    assert(parsed.response["error"] == "invalid_json");
  }

  {
    auto parsed = parseTtsRouteRequest(postJson(R"({"model":"voz.onnx"})"), 1024);
    assert(!parsed.ok);
    assert(parsed.status == 400);
    assert(parsed.response["error"] == "missing_fields");
  }

  {
    auto parsed = parseTtsRouteRequest(postJson(R"({"text":"abcdef"})"), 5);
    assert(!parsed.ok);
    assert(parsed.status == 413);
    assert(parsed.response["error"] == "payload_too_large");
  }

  {
    auto parsed = parseTtsRouteRequest(postJson(R"({"text":"Hola","speaker_id":"x"})"), 1024);
    assert(!parsed.ok);
    assert(parsed.status == 400);
    assert(parsed.response["error"] == "invalid_request");
  }

  {
    auto parsed = parseTtsRouteRequest(postJson(R"({"text":"Hola","noise_scale":9})"), 1024);
    assert(!parsed.ok);
    assert(parsed.status == 400);
    assert(parsed.response["error"] == "invalid_request");
  }

  {
    auto parsed = parseTtsRouteRequest(postJson(R"({"text":"Hola","output_file":"x.wav"})"), 1024);
    assert(!parsed.ok);
    assert(parsed.status == 400);
    assert(parsed.response["error"] == "invalid_request");
  }

  std::cout << "TEST_OK\n";
  return 0;
}
