find_package(glfw3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

add_executable(render-gl-selftest main.cpp)

set_target_properties(render-gl-selftest PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
)

target_include_directories(render-gl-selftest PRIVATE ${CMAKE_SOURCE_DIR}/vendor/stb)

target_link_libraries(render-gl-selftest PRIVATE
  synesthesia-vibe-render-gl
  beatdrop-parser
  beatdrop-audio
  beatdrop-platform-portable
  beatdrop-render-core
  projectM::ns-eel2
  glfw
  OpenGL::GL
)

if(MSVC)
  target_compile_options(render-gl-selftest PRIVATE /W3 /EHsc /MD)
else()
  target_compile_options(render-gl-selftest PRIVATE -std=c++17 -ffp-contract=off)
endif()