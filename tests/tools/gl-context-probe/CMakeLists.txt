find_package(glfw3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

add_executable(gl-context-probe gl-context-probe.cpp)

set_target_properties(gl-context-probe PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
)

target_include_directories(gl-context-probe PRIVATE ${CMAKE_SOURCE_DIR}/vendor/stb)

target_link_libraries(gl-context-probe PRIVATE glfw OpenGL::GL)

if(MSVC)
  target_compile_options(gl-context-probe PRIVATE /W3 /EHsc /MD)
else()
  target_compile_options(gl-context-probe PRIVATE -std=c++17 -ffp-contract=off)
endif()
