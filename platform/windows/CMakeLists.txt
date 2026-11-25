find_package(unofficial-angle CONFIG REQUIRED)
find_path(SYNESTHESIA_VIBE_ANGLE_INCLUDE_DIR EGL/eglext_angle.h REQUIRED)

add_library(synesthesia-vibe-windows SHARED
  src/audio_input.cpp
  src/audio_router.cpp
  src/egl_display.cpp
  src/frame_pacer.cpp
  src/host_support.cpp
  src/render_view.cpp
  src/snapshot_writer.cpp
  src/sync_group.cpp
  src/synesthesia_windows.cpp
  synesthesia-vibe.def
)

set_target_properties(synesthesia-vibe-windows PROPERTIES
  OUTPUT_NAME synesthesia-vibe
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
)

target_include_directories(synesthesia-vibe-windows PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/vendor/stb
  ${SYNESTHESIA_VIBE_ANGLE_INCLUDE_DIR}
  $<TARGET_PROPERTY:synesthesia-vibe-render-gl,INTERFACE_INCLUDE_DIRECTORIES>
)

target_compile_definitions(synesthesia-vibe-windows PRIVATE
  _WIN32_WINNT=0x0A00
  WINVER=0x0A00
)

target_link_libraries(synesthesia-vibe-windows PRIVATE
  SynesthesiaVibeCore
  unofficial::angle::libEGL
  ole32
)

target_compile_options(synesthesia-vibe-windows PRIVATE /W3 /EHsc /MD)

add_custom_command(TARGET synesthesia-vibe-windows POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:unofficial::angle::libEGL>
    $<TARGET_FILE:unofficial::angle::libGLESv2>
    $<TARGET_FILE_DIR:synesthesia-vibe-windows>
  VERBATIM
)
