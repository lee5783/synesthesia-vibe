add_library(synesthesiavibe-jni SHARED
  synesthesia_jni.cpp
  wav_feeder_jni.cpp
)

set_target_properties(synesthesiavibe-jni PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON
)

target_include_directories(synesthesiavibe-jni PRIVATE
  $<TARGET_PROPERTY:synesthesia-vibe-render-gl,INTERFACE_INCLUDE_DIRECTORIES>
)

target_link_libraries(synesthesiavibe-jni PRIVATE
  SynesthesiaVibeCore
  synesthesia-vibe-platform-default
  GLESv3
  EGL
  log
  android
)

target_compile_options(synesthesiavibe-jni PRIVATE -std=c++17 -ffp-contract=off)

target_link_options(synesthesiavibe-jni PRIVATE
  -Wl,--exclude-libs,ALL
  -Wl,--gc-sections
)
