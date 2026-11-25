cmake_minimum_required(VERSION 3.25)
project(SynesthesiaVibeWindowsSample LANGUAGES C)

set(SYNESTHESIA_VIBE_DIST "" CACHE PATH "A dist folder written by platform/windows/build-windows.sh, e.g. build/windows/dist-x64")

if(NOT EXISTS "${SYNESTHESIA_VIBE_DIST}/include/synesthesia-vibe/synesthesia-vibe-windows.h")
  message(FATAL_ERROR "Set SYNESTHESIA_VIBE_DIST to a dist folder written by platform/windows/build-windows.sh (got '${SYNESTHESIA_VIBE_DIST}').")
endif()

add_library(SynesthesiaVibe::Windows SHARED IMPORTED)
set_target_properties(SynesthesiaVibe::Windows PROPERTIES
  IMPORTED_LOCATION "${SYNESTHESIA_VIBE_DIST}/bin/synesthesia-vibe.dll"
  IMPORTED_IMPLIB "${SYNESTHESIA_VIBE_DIST}/lib/synesthesia-vibe.lib"
  INTERFACE_INCLUDE_DIRECTORIES "${SYNESTHESIA_VIBE_DIST}/include"
)

add_executable(synesthesia-vibe-windows-sample src/main.c)

set_target_properties(synesthesia-vibe-windows-sample PROPERTIES
  C_STANDARD 11
  C_STANDARD_REQUIRED ON
)

target_compile_definitions(synesthesia-vibe-windows-sample PRIVATE
  _WIN32_WINNT=0x0A00
  WINVER=0x0A00
)

target_link_libraries(synesthesia-vibe-windows-sample PRIVATE SynesthesiaVibe::Windows)

if(MSVC)
  target_compile_options(synesthesia-vibe-windows-sample PRIVATE /W3)
endif()

file(GLOB SYNESTHESIA_VIBE_DIST_DLLS "${SYNESTHESIA_VIBE_DIST}/bin/*.dll")

add_custom_command(TARGET synesthesia-vibe-windows-sample POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SYNESTHESIA_VIBE_DIST_DLLS} $<TARGET_FILE_DIR:synesthesia-vibe-windows-sample>
  VERBATIM
)
