add_executable(synesthesia-vibe-headless main.cpp)

set_target_properties(synesthesia-vibe-headless PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
)

target_include_directories(synesthesia-vibe-headless PRIVATE ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(synesthesia-vibe-headless PRIVATE
  synesthesia-vibe
  projectM::ns-eel2
)

if(MSVC)
  target_compile_options(synesthesia-vibe-headless PRIVATE /W3 /EHsc /MD /Zc:wchar_t)
else()
  target_compile_options(synesthesia-vibe-headless PRIVATE -std=c++17 -ffp-contract=off)
endif()