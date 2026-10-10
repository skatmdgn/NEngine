add_library(NEnginePhysics STATIC
  src/components.cpp
  src/registration.cpp
)

target_include_directories(NEnginePhysics
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(NEnginePhysics
  PUBLIC
    NEngineCore
)

target_compile_features(NEnginePhysics PUBLIC cxx_std_20)

if(MSVC)
  target_compile_options(NEnginePhysics PRIVATE /W4 /permissive-)
else()
  target_compile_options(NEnginePhysics PRIVATE -Wall -Wextra -Wpedantic)
endif()
