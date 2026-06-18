#include <cassert>
#include <stdexcept>
#include <string>

#include "server/markup/markup_parser.hpp"
#include "server/markup/request_options.hpp"

using namespace piper_server;

int main() {
  assert(looksLikeMarkupTts("Hola <model model=\"voz\">mundo</model>"));
  assert(looksLikeMarkupTts("<silence ms=\"250\"/>"));
  assert(!looksLikeMarkupTts("texto plano"));

  auto attrs = parseLooseTagAttributes("model=\"es_MX\" speaker='2' noise_scale=\"0.6\"");
  assert(attrs["model"] == "es_MX");
  assert(attrs["speaker"] == "2");
  assert(attrs["noise_scale"] == "0.6");

  auto voice = parseMarkupModelSettings(attrs);
  assert(voice.model && *voice.model == "es_MX");
  assert(voice.speakerId && *voice.speakerId == 2);
  assert(voice.noiseScale && *voice.noiseScale > 0.59f && *voice.noiseScale < 0.61f);

  auto voiceFromHash = parseMarkupModelSettings(parseLooseTagAttributes("model=\"multi#4\""));
  assert(voiceFromHash.model && *voiceFromHash.model == "multi");
  assert(voiceFromHash.speakerId && *voiceFromHash.speakerId == 4);

  auto silence = parseSilenceDurationMs(parseLooseTagAttributes("silence=\"1.5s\""));
  assert(silence && *silence == 1500);

  auto parsed = parseMarkupScript("A <silence ms=\"100\"/> <model model=\"voz\" speaker=\"1\">B</model> C");
  assert(parsed.segments.size() == 4);
  assert(parsed.segments[0].type == MarkupSegment::Type::Speech);
  assert(parsed.segments[1].type == MarkupSegment::Type::Silence);
  assert(parsed.segments[1].silenceMs == 100);
  assert(parsed.segments[2].voice.model && *parsed.segments[2].voice.model == "voz");
  assert(parsed.segments[2].voice.speakerId && *parsed.segments[2].voice.speakerId == 1);

  bool threw = false;
  try {
    parseMarkupScript("<silence/>");
  } catch (const std::runtime_error &e) {
    threw = std::string(e.what()) == "markup_silence_missing_duration";
  }
  assert(threw);

  json input = {{"noise_scale", 0.5}};
  auto req = requestFloatOption(input, {"noise_scale"}, "noise_scale", 0.0f, 1.0f);
  assert(req && *req > 0.49f && *req < 0.51f);

  return 0;
}
