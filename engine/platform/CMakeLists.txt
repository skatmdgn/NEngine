add_library(NEnginePlatform STATIC)

target_include_directories(NEnginePlatform
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(NEnginePlatform PUBLIC cxx_std_20)

if(WIN32)
  target_sources(NEnginePlatform PRIVATE src/window_win32.cpp)
  target_compile_definitions(NEnginePlatform PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_link_libraries(NEnginePlatform PRIVATE user32)
else()
  target_sources(NEnginePlatform PRIVATE src/window_stub.cpp)
endif()

if(MSVC)
  target_compile_options(NEnginePlatform PRIVATE /W4 /permissive-)
else()
  target_compile_options(NEnginePlatform PRIVATE -Wall -Wextra -Wpedantic)
endif()
