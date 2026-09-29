add_executable(NEngineEditor main.cpp)
target_link_libraries(NEngineEditor PRIVATE NEngineCore NEnginePlatform NEngineEditorModel)
target_compile_features(NEngineEditor PRIVATE cxx_std_20)

if(WIN32)
  target_sources(NEngineEditor PRIVATE win32_editor_shell.cpp)
  target_compile_definitions(NEngineEditor PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_link_libraries(NEngineEditor PRIVATE comctl32 gdi32)
endif()
