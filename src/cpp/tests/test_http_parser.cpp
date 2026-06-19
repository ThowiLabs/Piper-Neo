#include "server/http.hpp"

#include <cassert>
#include <iostream>
#include <string>

int main() {
  using namespace piper_server;

  {
    auto target = parseTarget("/api/v1/tts?model=es_MX&voice=A+B&empty=&encoded=hola%20mundo");
    assert(target.path == "/api/v1/tts");
    assert(queryValue(target, "model").value() == "es_MX");
    assert(queryValue(target, "VOICE").value() == "A B");
    assert(queryValue(target, "empty").value().empty());
    assert(queryValue(target, "encoded").value() == "hola mundo");
    assert(!queryValue(target, "missing").has_value());
  }

  {
    auto fileName = routeFileName("/api/v1/files/audio%20final.wav");
    assert(fileName.has_value());
    assert(fileName.value() == "audio final.wav");
    assert(!routeFileName("/api/v1/models/audio.wav").has_value());
  }

  {
    auto modelName = routeModelImageName("/api/v1/models/es_MX%20cortana/image");
    assert(modelName.has_value());
    assert(modelName.value() == "es_MX cortana");
    assert(!routeModelImageName("/api/v1/models/es_MX").has_value());
    assert(!routeModelImageName("/api/v1/files/es_MX/image").has_value());
  }

  std::cout << "TEST_OK\n";
  return 0;
}
