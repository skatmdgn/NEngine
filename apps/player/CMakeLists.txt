if(WIN32)
  add_executable(NEnginePlayer WIN32 winmain_win32.cpp)
  target_compile_definitions(NEnginePlayer PRIVATE
    UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_link_libraries(NEnginePlayer PRIVATE
    NEnginePlayerKit NEnginePlatform NEngineRender shell32 user32)
else()
  add_executable(NEnginePlayer main.cpp)
  target_link_libraries(NEnginePlayer PRIVATE
    NEnginePlayerKit)
endif()
target_compile_features(NEnginePlayer PRIVATE cxx_std_20)

# Authoring-side CLI, never linked into the shipped Player executable.
add_executable(NEnginePlayerBuild export_main.cpp)
target_compile_features(NEnginePlayerBuild PRIVATE cxx_std_20)
target_link_libraries(NEnginePlayerBuild PRIVATE NEnginePlayerKit NEngineEditorModel)
