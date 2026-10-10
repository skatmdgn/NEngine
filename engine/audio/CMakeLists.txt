add_library(NEngineAudio STATIC
  src/audio_clip.cpp
  src/audio_device_null.cpp
  src/audio_renderer.cpp
  src/clip_cache.cpp
  src/mix_snapshot.cpp
  src/playback.cpp
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
