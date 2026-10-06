add_library(NEngineAssets STATIC
  src/asset_guid.cpp
  src/asset_importer.cpp
  src/asset_database.cpp
  src/builtin_processors.cpp
  src/file_watcher.cpp
  src/gltf_sidecars.cpp
  src/import_pipeline.cpp
  src/dependency_graph.cpp
)

target_include_directories(NEngineAssets
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(NEngineAssets PUBLIC cxx_std_20)

if(MSVC)
  target_compile_options(NEngineAssets PRIVATE /W4 /permissive-)
else()
  target_compile_options(NEngineAssets PRIVATE -Wall -Wextra -Wpedantic)
endif()
