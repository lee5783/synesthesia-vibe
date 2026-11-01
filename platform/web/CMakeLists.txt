add_executable(synesthesia-vibe-web
  src/synesthesia_web.cpp
  src/wav_feeder_web.cpp
)

set_target_properties(synesthesia-vibe-web PROPERTIES
  OUTPUT_NAME synesthesia-vibe-wasm
  SUFFIX .js
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
)

target_include_directories(synesthesia-vibe-web PRIVATE
  $<TARGET_PROPERTY:synesthesia-vibe-render-gl,INTERFACE_INCLUDE_DIRECTORIES>
)

target_link_libraries(synesthesia-vibe-web PRIVATE
  SynesthesiaVibeCore
  synesthesia-vibe-platform-default
)

target_compile_options(synesthesia-vibe-web PRIVATE -std=c++17 -ffp-contract=off -fwasm-exceptions)

target_link_options(synesthesia-vibe-web PRIVATE
  -fwasm-exceptions
  -sMODULARIZE=1
  -sEXPORT_ES6=1
  -sEXPORT_NAME=createSynesthesiaVibeModule
  -sENVIRONMENT=web
  -sMIN_WEBGL_VERSION=2
  -sMAX_WEBGL_VERSION=2
  -sGL_ENABLE_GET_PROC_ADDRESS
  -sALLOW_MEMORY_GROWTH=1
  -sFORCE_FILESYSTEM=1
  -sSTACK_SIZE=1048576
  -sEXPORTED_FUNCTIONS=_malloc,_free
  -sEXPORTED_RUNTIME_METHODS=FS,GL,UTF8ToString,stringToNewUTF8,HEAPU8,HEAP16,HEAP32,HEAPF32
  $<$<CONFIG:Release>:-O2>
  $<$<CONFIG:Release>:-sASSERTIONS=0>
)
