if(NOT DEFINED FIXTURE OR NOT DEFINED BUILDER OR NOT DEFINED PLAYER OR
   NOT DEFINED TEST_ROOT)
  message(FATAL_ERROR "Player CLI smoke args missing")
endif()
file(REMOVE_RECURSE "${TEST_ROOT}")
file(MAKE_DIRECTORY "${TEST_ROOT}/Dist")
execute_process(COMMAND "${FIXTURE}" "${TEST_ROOT}/Project"
                RESULT_VARIABLE generated)
if(NOT generated EQUAL 0)
  message(FATAL_ERROR "Fixture generator failed: ${generated}")
endif()
execute_process(COMMAND "${BUILDER}" "${TEST_ROOT}/Project"
                        "${TEST_ROOT}/Dist/Game" "${PLAYER}"
                RESULT_VARIABLE exported OUTPUT_VARIABLE output
                ERROR_VARIABLE error)
if(NOT exported EQUAL 0)
  message(FATAL_ERROR "Player exporter failed: ${exported}: ${error}")
endif()
if(NOT EXISTS "${TEST_ROOT}/Dist/Game/NEngine.player" OR
   NOT EXISTS "${TEST_ROOT}/Dist/Game/Content/Main.nscene")
  message(FATAL_ERROR "Player export missing manifest or startup Scene")
endif()
if(WIN32)
  if(NOT EXISTS "${TEST_ROOT}/Dist/Game/NEnginePlayer.exe")
    message(FATAL_ERROR "Windows Player executable missing")
  endif()
else()
  execute_process(COMMAND "${TEST_ROOT}/Dist/Game/NEnginePlayer"
                          "${TEST_ROOT}/Dist/Game"
                  RESULT_VARIABLE launched OUTPUT_VARIABLE player_output
                  ERROR_VARIABLE player_error)
  if(NOT launched EQUAL 0)
    message(FATAL_ERROR "Portable Player bootstrap failed: ${player_error}")
  endif()
endif()
message(STATUS "Player CLI export/launch integration passed")
