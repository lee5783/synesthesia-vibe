add_library(synesthesia-vibe-platform-default STATIC
  wav_reader.cpp
)

set_target_properties(synesthesia-vibe-platform-default PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
)

target_include_directories(synesthesia-vibe-platform-default PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)

if(MSVC)
  target_compile_options(synesthesia-vibe-platform-default PRIVATE /W3 /EHsc /MD)
else()
  target_compile_options(synesthesia-vibe-platform-default PRIVATE -std=c++17 -ffp-contract=off)
endif()
