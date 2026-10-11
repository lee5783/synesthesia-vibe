find_package(GTest CONFIG REQUIRED)

set(SYNESTHESIA_VIBE_PHASE2_REFERENCE_DIR "${CMAKE_SOURCE_DIR}/tests/golden/phase2-reference" CACHE PATH
  "Directory holding the P2-017 --dump-audio reference data (gitignored, local to this machine; override to point at a re-extracted archive copy)")

if(MSVC)
  set(BEATDROP_UNIT_WAV_SOURCES ${BEATDROP_APPS_HOST_DIR}/wavaudiosource.cpp)
  set(BEATDROP_UNIT_WAV_INCLUDE_DIRS ${BEATDROP_APPS_HOST_DIR})
else()
  set(BEATDROP_UNIT_WAV_SOURCES ${CMAKE_SOURCE_DIR}/platform/default/wav_reader.cpp)
  set(BEATDROP_UNIT_WAV_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/platform/default)
endif()

add_executable(synesthesia-vibe-unit-tests
  smoke_test.cpp
  rng_compat_test.cpp
  audio_fft_loudness_test.cpp
  projectm_eval_conformance_test.cpp
  noise_level0_test.cpp
  ${BEATDROP_SRC_DIR}/platform/rng_compat.cpp
  ${BEATDROP_SRC_DIR}/platform/nseel_hoststub.cpp
  ${BEATDROP_SRC_DIR}/render/core/noise.cpp
  ${BEATDROP_UNIT_WAV_SOURCES}
)

set_target_properties(synesthesia-vibe-unit-tests PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
)

target_include_directories(synesthesia-vibe-unit-tests PRIVATE ${BEATDROP_SRC_DIR}/platform ${BEATDROP_SRC_DIR}/render/core ${BEATDROP_UNIT_WAV_INCLUDE_DIRS})
target_compile_definitions(synesthesia-vibe-unit-tests PRIVATE
  SYNESTHESIA_VIBE_RNG_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
  SYNESTHESIA_VIBE_PHASE2_REFERENCE_DIR="${SYNESTHESIA_VIBE_PHASE2_REFERENCE_DIR}"
  SYNESTHESIA_VIBE_PHASE2_WAV_PATH="${CMAKE_SOURCE_DIR}/tests/tools/wav-generator/deterministic-test.wav"
  SYNESTHESIA_VIBE_MVP_PRESETS_DIR="${CMAKE_SOURCE_DIR}/resources/mvp-presets"
  SYNESTHESIA_VIBE_GOLDEN_CORPUS_DIR="${CMAKE_SOURCE_DIR}/tests/golden/corpus"
)

target_link_libraries(synesthesia-vibe-unit-tests PRIVATE beatdrop-audio projectM::ns-eel2 GTest::gtest GTest::gtest_main)

if(MSVC)
  target_compile_options(synesthesia-vibe-unit-tests PRIVATE /W3 /EHsc /MD)
else()
  target_compile_options(synesthesia-vibe-unit-tests PRIVATE -std=c++17 -ffp-contract=off)
endif()

include(GoogleTest)
gtest_discover_tests(synesthesia-vibe-unit-tests)
