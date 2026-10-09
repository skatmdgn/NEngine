add_library(NEngineScripting STATIC
  src/dotnet_host.cpp
  src/managed_build.cpp
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

if(UNIX AND NOT APPLE)
  target_link_libraries(NEngineScripting PRIVATE dl)
endif()
