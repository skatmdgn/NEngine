add_library(NEnginePlayerKit STATIC
  src/bundle.cpp
)
target_include_directories(NEnginePlayerKit PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(NEnginePlayerKit PUBLIC NEngineCore)
target_compile_features(NEnginePlayerKit PUBLIC cxx_std_20)
