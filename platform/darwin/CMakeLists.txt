enable_language(OBJCXX)

add_library(synesthesia-vibe-darwin OBJECT
  src/SVContextConfiguration.mm
  src/SVContext.mm
  src/SVAudioInput.mm
  src/SVSystemAudioInput.mm
  src/SVWavFeeder.mm
  src/SVCaptureSession.mm
  ../macos/SVView+macOS.mm
)

set_target_properties(synesthesia-vibe-darwin PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  OBJCXX_STANDARD 17
  OBJCXX_STANDARD_REQUIRED ON
)

target_include_directories(synesthesia-vibe-darwin
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(synesthesia-vibe-darwin PRIVATE
  synesthesia-vibe-render-gl
  synesthesia-vibe-platform-default
)

target_compile_definitions(synesthesia-vibe-darwin PRIVATE NDEBUG GL_SILENCE_DEPRECATION GLES_SILENCE_DEPRECATION)

target_compile_options(synesthesia-vibe-darwin PRIVATE -fobjc-arc -std=c++17 -ffp-contract=off)

target_sources(SynesthesiaVibeCore PRIVATE
  $<TARGET_OBJECTS:synesthesia-vibe-darwin>
  $<TARGET_OBJECTS:synesthesia-vibe-platform-default>
)

target_include_directories(SynesthesiaVibeCore PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include/SynesthesiaVibeCore>
)

target_link_libraries(SynesthesiaVibeCore INTERFACE
  "-framework Foundation"
  "-framework AVFoundation"
)

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  target_link_libraries(SynesthesiaVibeCore INTERFACE
    "-framework CoreAudio"
    "-framework OpenGL"
    "-framework AppKit"
    "-framework CoreVideo"
    "-framework QuartzCore"
  )
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
  target_sources(synesthesia-vibe-darwin PRIVATE ../ios/SVView+iOS.mm)

  target_link_libraries(SynesthesiaVibeCore INTERFACE
    "-framework UIKit"
    "-framework QuartzCore"
  )
endif()

include(GNUInstallDirs)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/SynesthesiaVibeCore
)
