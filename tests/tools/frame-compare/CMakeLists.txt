add_executable(frame-compare frame-compare.cpp)

set_target_properties(frame-compare PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
)

target_include_directories(frame-compare PRIVATE
  ${CMAKE_SOURCE_DIR}/vendor/stb
)

if(MSVC)
  target_compile_options(frame-compare PRIVATE /W3 /EHsc /MD)
else()
  target_compile_options(frame-compare PRIVATE -std=c++17 -ffp-contract=off)
endif()
