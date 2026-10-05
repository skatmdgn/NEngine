add_library(NEngineScripting STATIC
  src/managed_project.cpp
)

target_include_directories(NEngineScripting
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(NEngineScripting PUBLIC cxx_std_20)

if(MSVC)
  target_compile_options(NEngineScripting PRIVATE /W4 /permissive-)
else()
  target_compile_options(NEngineScripting PRIVATE -Wall -Wextra -Wpedantic)
endif()
