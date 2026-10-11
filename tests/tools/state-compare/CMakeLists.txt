add_executable(state-compare state-compare.cpp)

set_target_properties(state-compare PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
)

if(MSVC)
  target_compile_options(state-compare PRIVATE /W3 /EHsc /MD)
else()
  target_compile_options(state-compare PRIVATE -std=c++17 -ffp-contract=off)
endif()