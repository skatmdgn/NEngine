add_library(NEngineRender STATIC
  src/registration.cpp
  src/render_snapshot.cpp
)

target_include_directories(NEngineRender
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(NEngineRender
  PUBLIC
    NEngineCore
    NEngineAssets
)

target_compile_features(NEngineRender PUBLIC cxx_std_20)

if(MSVC)
  target_compile_options(NEngineRender PRIVATE /W4 /permissive-)
else()
  target_compile_options(NEngineRender PRIVATE -Wall -Wextra -Wpedantic)
endif()
