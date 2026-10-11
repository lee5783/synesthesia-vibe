cmake_minimum_required(VERSION 3.25)
project(hlsl-survey LANGUAGES CXX)

set(HLSLPARSER_DIR ${CMAKE_SOURCE_DIR}/../../../vendor/hlslparser/src)

add_executable(hlsl-survey
  hlsl-survey.cpp
  ${HLSLPARSER_DIR}/CodeWriter.cpp
  ${HLSLPARSER_DIR}/Engine.cpp
  ${HLSLPARSER_DIR}/GLSLGenerator.cpp
  ${HLSLPARSER_DIR}/HLSLParser.cpp
  ${HLSLPARSER_DIR}/HLSLTokenizer.cpp
  ${HLSLPARSER_DIR}/HLSLTree.cpp
)

set_target_properties(hlsl-survey PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
)

target_include_directories(hlsl-survey PRIVATE
  ${HLSLPARSER_DIR}
)

if(MSVC)
  target_compile_options(hlsl-survey PRIVATE /W3 /EHsc /MD)
endif()
