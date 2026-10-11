add_executable(platform-link-check
  main.cpp
  $<TARGET_OBJECTS:beatdrop-parser>
  $<TARGET_OBJECTS:beatdrop-platform-portable>
)

set_target_properties(platform-link-check PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
)

target_include_directories(platform-link-check PRIVATE
  ${BEATDROP_SRC_DIR}/parser
  ${BEATDROP_SRC_DIR}/platform
  ${BEATDROP_SRC_DIR}/config
)

target_link_libraries(platform-link-check PRIVATE
  projectM::ns-eel2
)

if(MSVC)
  target_compile_options(platform-link-check PRIVATE /W3 /EHsc /MD /Zc:wchar_t)
else()
  target_compile_options(platform-link-check PRIVATE -std=c++17 -ffp-contract=off)
endif()
