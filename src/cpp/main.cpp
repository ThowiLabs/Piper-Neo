#include <cstdlib>
#include <exception>
#include <iostream>

#include "app/piper_app.hpp"
#include "app/platform.hpp"

int main(int argc, char *argv[]) {
  piper_app::configureConsoleUtf8();

  try {
    return piper_app::piperMain(argc, argv);
  } catch (const std::exception &error) {
    std::cerr << "piper: error: " << error.what() << std::endl;
    std::cerr << "Run with --help to see available options." << std::endl;
    return EXIT_FAILURE;
  } catch (...) {
    std::cerr << "piper: error: unknown fatal error" << std::endl;
    std::cerr << "Run with --help to see available options." << std::endl;
    return EXIT_FAILURE;
  }
}
