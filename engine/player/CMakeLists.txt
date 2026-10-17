add_library(NEnginePlayerKit STATIC
  src/bundle.cpp
  src/assets.cpp
  src/exporter.cpp
  src/runtime.cpp
  src/managed_queries.cpp
  src/managed_properties.cpp
  src/managed_render_properties.cpp
  src/managed_audio_properties.cpp
)
target_include_directories(NEnginePlayerKit PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(NEnginePlayerKit PUBLIC NEngineCore NEngineAssets NEngineRender NEnginePhysics NEngineAudio NEngineScripting NEngineInput)
target_compile_features(NEnginePlayerKit PUBLIC cxx_std_20)
