add_library(synesthesia-vibe STATIC
  context.cpp
  host_sink.cpp
  preset.cpp
  audio.cpp
  frame.cpp
  frame_state.cpp
  gl_pipeline.cpp
  gl_frame.cpp
)

set_target_properties(synesthesia-vibe PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
)

target_include_directories(synesthesia-vibe PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${BEATDROP_SRC_DIR}/render/core
  ${BEATDROP_SRC_DIR}/parser
  ${BEATDROP_SRC_DIR}/config
  ${BEATDROP_SRC_DIR}/platform
  ${BEATDROP_SRC_DIR}/audio
)

target_link_libraries(synesthesia-vibe PRIVATE
  beatdrop-parser
  beatdrop-audio
  beatdrop-platform-portable
  beatdrop-render-core
  synesthesia-vibe-render-gl
  projectM::ns-eel2
)

target_compile_definitions(synesthesia-vibe PRIVATE
  NDEBUG
  STRSAFE_NO_DEPRECATE
  NSEEL_REENTRANT_EXECUTION
  _CRT_SECURE_NO_WARNINGS
  _SILENCE_EXPERIMENTAL_COROUTINE_DEPRECATION_WARNINGS
  _MBCS
)

if(MSVC)
  target_compile_options(synesthesia-vibe PRIVATE
    /Zi /nologo /W3 /WX- /diagnostics:column /O1 /Ob2 /Oy /GF /Gm- /EHsc /MD /GS- /Gy
    /fp:precise /Zc:wchar_t /Zc:forScope- /Zc:inline /std:c++17 /external:W3 /Gd /TP /wd4996
    /analyze- /FC
  )
else()
  target_compile_options(synesthesia-vibe PRIVATE -std=c++17 -ffp-contract=off)
endif()

if(SYNESTHESIA_VIBE_BACKEND STREQUAL "gles")
  target_compile_definitions(synesthesia-vibe PRIVATE SYNESTHESIA_VIBE_GLES=1)
endif()

if(SYNESTHESIA_VIBE_PORTABLE_ONLY)
  return()
endif()

add_library(SynesthesiaVibeCore STATIC
  $<TARGET_OBJECTS:synesthesia-vibe>
  $<TARGET_OBJECTS:synesthesia-vibe-render-gl>
  $<TARGET_OBJECTS:synesthesia-vibe-hlslparser>
  $<TARGET_OBJECTS:beatdrop-parser>
  $<TARGET_OBJECTS:beatdrop-audio>
  $<TARGET_OBJECTS:beatdrop-platform-portable>
  $<TARGET_OBJECTS:beatdrop-render-core>
)

set_target_properties(SynesthesiaVibeCore PROPERTIES
  OUTPUT_NAME SynesthesiaVibeCore
  LINKER_LANGUAGE CXX
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
)

target_include_directories(SynesthesiaVibeCore PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

if(SYNESTHESIA_VIBE_BACKEND STREQUAL "gles")
  target_compile_definitions(SynesthesiaVibeCore PUBLIC SYNESTHESIA_VIBE_GLES=1)

  if(APPLE)
    target_compile_definitions(SynesthesiaVibeCore PUBLIC GLES_SILENCE_DEPRECATION)
    target_compile_definitions(synesthesia-vibe PRIVATE GLES_SILENCE_DEPRECATION)
    target_link_libraries(SynesthesiaVibeCore INTERFACE "-framework OpenGLES")
  elseif(ANDROID)
    target_link_libraries(SynesthesiaVibeCore INTERFACE GLESv3 EGL)
  endif()
endif()

if(MSVC)
  target_compile_options(SynesthesiaVibeCore PRIVATE /W3 /EHsc /MD)
else()
  target_compile_options(SynesthesiaVibeCore PRIVATE -std=c++17 -ffp-contract=off)
endif()

if(APPLE)
  add_custom_command(TARGET SynesthesiaVibeCore POST_BUILD
    COMMAND xcrun libtool -static -no_warning_for_no_symbols -o $<TARGET_FILE:SynesthesiaVibeCore>.merged
      $<TARGET_FILE:SynesthesiaVibeCore>
      $<TARGET_FILE:projectM::ns-eel2>
      $<TARGET_FILE:projectM::Eval>
    COMMAND ${CMAKE_COMMAND} -E rename $<TARGET_FILE:SynesthesiaVibeCore>.merged $<TARGET_FILE:SynesthesiaVibeCore>
    VERBATIM
  )
else()
  target_link_libraries(SynesthesiaVibeCore INTERFACE
    projectM::ns-eel2
    projectM::Eval
  )
endif()

include(GNUInstallDirs)

install(TARGETS SynesthesiaVibeCore
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

if(NOT APPLE)
  install(FILES
    $<TARGET_FILE:projectM::ns-eel2>
    $<TARGET_FILE:projectM::Eval>
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
  )
endif()
