cmake_minimum_required(VERSION 3.13)

project(piper C CXX)

file(READ "${CMAKE_CURRENT_LIST_DIR}/VERSION" piper_version)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Prefer a self-contained Windows binary. Without this, Release artifacts can
# depend on Visual C++ runtime DLLs that may be missing on end-user machines.
if(POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW)
endif()
if(POLICY CMP0135)
  # ExternalProject downloads should use extraction timestamps so rebuilds are
  # reliable when an archive URL changes.
  cmake_policy(SET CMP0135 NEW)
endif()
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(MSVC)
  # Force compiler to use UTF-8 for IPA constants
  add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
  add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  # Linux flags
  string(APPEND CMAKE_CXX_FLAGS " -Wall -Wextra -Wl,-rpath,'$ORIGIN'")
  string(APPEND CMAKE_C_FLAGS " -Wall -Wextra")
elseif(MINGW)
  # Native Windows build with MinGW/WinLibs. Do not add Linux rpath flags here.
  string(APPEND CMAKE_CXX_FLAGS " -Wall -Wextra")
  string(APPEND CMAKE_C_FLAGS " -Wall -Wextra")
endif()

set(PIPER_APP_SOURCES
  src/cpp/app/cli_args.cpp
  src/cpp/app/env.cpp
  src/cpp/app/export_neo_mode.cpp
  src/cpp/app/hardware.cpp
  src/cpp/app/piper_app.cpp
  src/cpp/app/platform.cpp
  src/cpp/app/raw_audio_output.cpp
  src/cpp/app/server_mode.cpp
  src/cpp/app/synthesis_mode.cpp
  src/cpp/app/voice_runtime.cpp
)

set(PIPER_SERVER_SOURCES
  src/cpp/server.cpp
  src/cpp/server/auth.cpp
  src/cpp/server/http.cpp
  src/cpp/server/markup_tts.cpp
  src/cpp/server/model_cache.cpp
  src/cpp/server/model_registry.cpp
  src/cpp/server/output_cleanup.cpp
  src/cpp/server/request_handler.cpp
  src/cpp/server/responses.cpp
  src/cpp/server/text_sanitizer.cpp
  src/cpp/server/tts_scheduler.cpp
  src/cpp/server/utils.cpp
  src/cpp/server/wav_utils.cpp
)


set(PIPER_NEO_SOURCES
  src/cpp/neo/binary_io.cpp
  src/cpp/neo/compression.cpp
  src/cpp/neo/file_utils.cpp
  src/cpp/neo/image_payload.cpp
  src/cpp/neo/package_reader.cpp
  src/cpp/neo/package_writer.cpp
  src/cpp/neo_model.cpp
)

set(PIPER_TEXT_SOURCES
  src/cpp/text/builtin_normalizer.cpp
  src/cpp/text/replacements.cpp
  src/cpp/text/spanish_numbers.cpp
  src/cpp/text/string_utils.cpp
  src/cpp/text_normalizer.cpp
)

set(PIPER_CORE_SOURCES
  src/cpp/piper.cpp
  src/cpp/core/model_runtime.cpp
  src/cpp/core/piper_runtime.cpp
  src/cpp/core/sentence_splitter.cpp
  src/cpp/core/synthesis_pipeline.cpp
  src/cpp/core/synthesis_utils.cpp
  src/cpp/core/text_chunker.cpp
  src/cpp/core/voice_loader.cpp
  src/cpp/core/wav_stream_writer.cpp
  ${PIPER_NEO_SOURCES}
  ${PIPER_TEXT_SOURCES}
)

add_executable(piper
  src/cpp/main.cpp
  ${PIPER_APP_SOURCES}
  ${PIPER_CORE_SOURCES}
  ${PIPER_SERVER_SOURCES}
)

option(PIPER_BUILD_TESTS "Build the optional Piper test executable" OFF)
if(PIPER_BUILD_TESTS)
  add_executable(test_piper
    src/cpp/test.cpp
    src/cpp/piper.cpp
    src/cpp/core/model_runtime.cpp
    src/cpp/core/piper_runtime.cpp
    src/cpp/core/sentence_splitter.cpp
    src/cpp/core/synthesis_pipeline.cpp
    src/cpp/core/synthesis_utils.cpp
    src/cpp/core/text_chunker.cpp
    src/cpp/core/voice_loader.cpp
    src/cpp/core/wav_stream_writer.cpp
    ${PIPER_TEXT_SOURCES}
  )
endif()

# NOTE: external project prefix are shortened because of path length restrictions on Windows
# NOTE: onnxruntime is pulled from piper-phonemize

# ---- fmt ---

if(NOT DEFINED FMT_DIR)
  set(FMT_VERSION "10.0.0")
  set(FMT_DIR "${CMAKE_CURRENT_BINARY_DIR}/fi")

  include(ExternalProject)
  ExternalProject_Add(
    fmt_external
    PREFIX "${CMAKE_CURRENT_BINARY_DIR}/f"
    URL "https://github.com/fmtlib/fmt/archive/refs/tags/${FMT_VERSION}.zip"
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX:PATH=${FMT_DIR}
    CMAKE_ARGS -DFMT_TEST:BOOL=OFF  # Don't build all the tests
  )
  add_dependencies(piper fmt_external)
  if(PIPER_BUILD_TESTS)
    add_dependencies(test_piper fmt_external)
  endif()
endif()

# ---- spdlog ---

if(NOT DEFINED SPDLOG_DIR)
  set(SPDLOG_DIR "${CMAKE_CURRENT_BINARY_DIR}/si")
  set(SPDLOG_VERSION "1.12.0")
  ExternalProject_Add(
    spdlog_external
    PREFIX "${CMAKE_CURRENT_BINARY_DIR}/s"
    URL "https://github.com/gabime/spdlog/archive/refs/tags/v${SPDLOG_VERSION}.zip"
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX:PATH=${SPDLOG_DIR}
  )
  add_dependencies(piper spdlog_external)
  if(PIPER_BUILD_TESTS)
    add_dependencies(test_piper spdlog_external)
  endif()
endif()

# ---- piper-phonemize ---

if(NOT DEFINED PIPER_PHONEMIZE_DIR)
  set(PIPER_PHONEMIZE_DIR "${CMAKE_CURRENT_BINARY_DIR}/pi")
  ExternalProject_Add(
    piper_phonemize_external
    PREFIX "${CMAKE_CURRENT_BINARY_DIR}/p"
    URL "https://github.com/rhasspy/piper-phonemize/archive/refs/heads/master.zip"
    CMAKE_ARGS
      -DCMAKE_INSTALL_PREFIX:PATH=${PIPER_PHONEMIZE_DIR}
      -DBUILD_TESTING:BOOL=OFF
      -DPIPER_BUILD_TESTS:BOOL=OFF
  )
  add_dependencies(piper piper_phonemize_external)
  if(PIPER_BUILD_TESTS)
    add_dependencies(test_piper piper_phonemize_external)
  endif()
endif()

# ---- Declare executable ----

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_link_libraries(piper -static-libgcc -static-libstdc++)

  set(PIPER_EXTRA_LIBRARIES "pthread")
elseif(MINGW)
  # Keep MinGW/WinLibs builds native to Windows and avoid Linux-only rpath/pthread.
  target_link_options(piper PRIVATE -static-libgcc -static-libstdc++)
endif()

target_link_libraries(piper
  fmt
  spdlog
  espeak-ng
  piper_phonemize
  onnxruntime
  ${PIPER_EXTRA_LIBRARIES}
)

if(WIN32)
  target_link_libraries(piper ws2_32)
endif()

# ---- zstd for Piper Neo model packages ---

find_package(zstd CONFIG QUIET)
if(zstd_FOUND)
  if(TARGET zstd::libzstd_static)
    target_link_libraries(piper zstd::libzstd_static)
  elseif(TARGET zstd::libzstd_shared)
    target_link_libraries(piper zstd::libzstd_shared)
  elseif(TARGET zstd::libzstd)
    target_link_libraries(piper zstd::libzstd)
  else()
    message(FATAL_ERROR "zstd package was found, but no known zstd CMake target exists")
  endif()
  target_compile_definitions(piper PUBLIC PIPER_NEO_HAS_ZSTD=1)
else()
  find_path(ZSTD_INCLUDE_DIR zstd.h)
  find_library(ZSTD_LIBRARY NAMES zstd libzstd)
  if(ZSTD_INCLUDE_DIR AND ZSTD_LIBRARY)
    target_include_directories(piper PUBLIC ${ZSTD_INCLUDE_DIR})
    target_link_libraries(piper ${ZSTD_LIBRARY})
    target_compile_definitions(piper PUBLIC PIPER_NEO_HAS_ZSTD=1)
  else()
    message(WARNING "zstd was not found. Piper Neo .neo zstd import/export will compile but fail at runtime.")
  endif()
endif()

target_link_directories(piper PUBLIC
  ${FMT_DIR}/lib
  ${SPDLOG_DIR}/lib
  ${PIPER_PHONEMIZE_DIR}/lib
)

target_include_directories(piper PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src/cpp
  ${FMT_DIR}/include
  ${SPDLOG_DIR}/include
  ${PIPER_PHONEMIZE_DIR}/include
)

target_compile_definitions(piper PUBLIC _PIPER_VERSION=${piper_version})

# ---- Declare optional tests ----
if(PIPER_BUILD_TESTS)
  include(CTest)
  enable_testing()

  set(PIPER_TEST_VOICE "${CMAKE_SOURCE_DIR}/etc/test_voice.onnx" CACHE FILEPATH "Optional ONNX voice used by ctest")
  if(EXISTS "${PIPER_TEST_VOICE}")
    add_test(
      NAME test_piper
      COMMAND test_piper "${PIPER_TEST_VOICE}" "${PIPER_PHONEMIZE_DIR}/share/espeak-ng-data" "${CMAKE_CURRENT_BINARY_DIR}/test.wav"
    )
  else()
    message(WARNING "PIPER_BUILD_TESTS is ON, but PIPER_TEST_VOICE was not found. test_piper will build, but ctest will not register the sample voice test.")
  endif()

  target_compile_features(test_piper PUBLIC cxx_std_17)

  target_include_directories(
    test_piper PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cpp
    ${FMT_DIR}/include
    ${SPDLOG_DIR}/include
    ${PIPER_PHONEMIZE_DIR}/include
  )

  target_link_directories(
    test_piper PUBLIC
    ${FMT_DIR}/lib
    ${SPDLOG_DIR}/lib
    ${PIPER_PHONEMIZE_DIR}/lib
  )

  target_link_libraries(test_piper PUBLIC
    fmt
    spdlog
    espeak-ng
    piper_phonemize
    onnxruntime
  )
endif()

# ---- Declare install targets ----

install(
  TARGETS piper
  DESTINATION ${CMAKE_INSTALL_PREFIX})

# Dependencies
install(
  DIRECTORY ${PIPER_PHONEMIZE_DIR}/bin/
  DESTINATION ${CMAKE_INSTALL_PREFIX}
  USE_SOURCE_PERMISSIONS  # keep +x
  FILES_MATCHING
  PATTERN "piper_phonemize"
  PATTERN "espeak-ng"
  PATTERN "*.dll"
)

install(
  DIRECTORY ${PIPER_PHONEMIZE_DIR}/lib/
  DESTINATION ${CMAKE_INSTALL_PREFIX}
  FILES_MATCHING
  PATTERN "*.dll"
  PATTERN "*.so*"
)

install(
  DIRECTORY ${PIPER_PHONEMIZE_DIR}/share/espeak-ng-data
  DESTINATION ${CMAKE_INSTALL_PREFIX}
)

install(
  FILES ${PIPER_PHONEMIZE_DIR}/share/libtashkeel_model.ort
  DESTINATION ${CMAKE_INSTALL_PREFIX}
)
