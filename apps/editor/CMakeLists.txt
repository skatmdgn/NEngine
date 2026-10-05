if(WIN32)
  add_executable(NEngineEditor WIN32
    winmain_win32.cpp
    win32_editor_shell.cpp
  )
  target_compile_definitions(NEngineEditor PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_link_libraries(NEngineEditor PRIVATE NEngineCore NEnginePlatform NEngineEditorModel NEngineScripting comdlg32 gdi32 shell32)
else()
  add_executable(NEngineEditor main.cpp)
  target_link_libraries(NEngineEditor PRIVATE NEngineCore NEnginePlatform NEngineEditorModel NEngineScripting)
endif()

target_compile_features(NEngineEditor PRIVATE cxx_std_20)
