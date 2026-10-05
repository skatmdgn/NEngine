add_library(NEngineCore STATIC
  src/world.cpp
  src/component_registry.cpp
  src/component_serialization.cpp
  src/scene.cpp
  src/prefab.cpp
)

target_include_directories(NEngineCore
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(NEngineCore PUBLIC cxx_std_20)

if(MSVC)
  target_compile_options(NEngineCore PRIVATE /W4 /permissive-)
else()
  target_compile_options(NEngineCore PRIVATE -Wall -Wextra -Wpedantic)
endif()
