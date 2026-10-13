add_library(NEngineUI STATIC
  src/canvas.cpp
)
target_include_directories(NEngineUI PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(NEngineUI PUBLIC NEngineCore)
target_compile_features(NEngineUI PUBLIC cxx_std_20)
if(MSVC)
  target_compile_options(NEngineUI PRIVATE /W4 /permissive-)
else()
  target_compile_options(NEngineUI PRIVATE -Wall -Wextra -Wpedantic)
endif()
