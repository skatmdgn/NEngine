add_library(NEngineEditorModel STATIC
  src/command.cpp
  src/play_session.cpp
)

target_include_directories(NEngineEditorModel
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(NEngineEditorModel PUBLIC NEngineCore)
target_compile_features(NEngineEditorModel PUBLIC cxx_std_20)

if(MSVC)
  target_compile_options(NEngineEditorModel PRIVATE /W4 /permissive-)
else()
  target_compile_options(NEngineEditorModel PRIVATE -Wall -Wextra -Wpedantic)
endif()
