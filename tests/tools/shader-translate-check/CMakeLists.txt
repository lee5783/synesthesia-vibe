find_package(glfw3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

add_executable(shader-translate-check main.cpp bindings_check.cpp)

set_target_properties(shader-translate-check PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
)

target_link_libraries(shader-translate-check PRIVATE
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
  target_compile_options(shader-translate-check PRIVATE /W3 /EHsc /MD)
else()
  target_compile_options(shader-translate-check PRIVATE -std=c++17 -ffp-contract=off)
endif()
