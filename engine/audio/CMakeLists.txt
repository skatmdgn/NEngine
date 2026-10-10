add_library(NEngineAudio STATIC
  src/mix_snapshot.cpp
  src/registration.cpp
)

target_include_directories(NEngineAudio
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(NEngineAudio
  PUBLIC
    NEngineCore
    NEngineAssets
)

target_compile_features(NEngineAudio PUBLIC cxx_std_20)

if(MSVC)
  target_compile_options(NEngineAudio PRIVATE /W4 /permissive-)
else()
  target_compile_options(NEngineAudio PRIVATE -Wall -Wextra -Wpedantic)
endif()
