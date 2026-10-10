add_library(NEngineEditorModel STATIC
  src/audio_integration.cpp
  src/command.cpp
  src/component_factory.cpp
  src/console_model.cpp
  src/editor_layout.cpp
  src/editor_model.cpp
  src/play_session.cpp
  src/physics_integration.cpp
  src/property_access.cpp
  src/property_command.cpp
  src/property_text.cpp
  src/project_manifest.cpp
  src/project_session.cpp
  src/presentation.cpp
  src/render_integration.cpp
  src/scene_interaction.cpp
  src/scripting_integration.cpp
)

target_include_directories(NEngineEditorModel
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(NEngineEditorModel PUBLIC NEngineCore NEngineAssets NEngineAudio NEnginePhysics NEngineRender NEngineScripting NEngineInput)
target_compile_features(NEngineEditorModel PUBLIC cxx_std_20)

if(MSVC)
  target_compile_options(NEngineEditorModel PRIVATE /W4 /permissive-)
else()
  target_compile_options(NEngineEditorModel PRIVATE -Wall -Wextra -Wpedantic)
endif()
