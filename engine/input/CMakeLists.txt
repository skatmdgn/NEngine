add_library(NEngineInput STATIC
  src/input_state.cpp
)

target_include_directories(NEngineInput
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(NEngineInput PUBLIC cxx_std_20)

if(MSVC)
  target_compile_options(NEngineInput PRIVATE /W4 /permissive-)
else()
  target_compile_options(NEngineInput PRIVATE -Wall -Wextra -Wpedantic)
endif()
