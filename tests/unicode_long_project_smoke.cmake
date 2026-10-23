if(NOT DEFINED FIXTURE OR NOT DEFINED BUILDER OR NOT DEFINED PLAYER OR
   NOT DEFINED TEST_ROOT)
  message(FATAL_ERROR "Unicode path acceptance args missing")
endif()
set(deep "한글 경로 길이 공백 검사")
set(root "${TEST_ROOT}/${deep}/${deep}/${deep}/${deep}/${deep}/${deep}")
file(REMOVE_RECURSE "${TEST_ROOT}")
file(MAKE_DIRECTORY "${TEST_ROOT}/Out")
execute_process(COMMAND "${FIXTURE}" "${root}"
                RESULT_VARIABLE generated
                OUTPUT_VARIABLE gen_out ERROR_VARIABLE gen_err)
if(NOT generated EQUAL 0)
  message(FATAL_ERROR "QA-006 failed to generate UTF-8/long-path project: ${gen_err}")
endif()
set(destination "${TEST_ROOT}/Out/한글 게임")
execute_process(COMMAND "${BUILDER}" "${root}" "${destination}" "${PLAYER}"
                RESULT_VARIABLE exported
                OUTPUT_VARIABLE export_out ERROR_VARIABLE export_err)
if(NOT exported EQUAL 0)
  message(FATAL_ERROR "QA-006 failed to export Unicode project: ${export_err}")
endif()
if(NOT EXISTS "${destination}/NEngine.player" OR
   NOT EXISTS "${destination}/Content/Main.nscene")
  message(FATAL_ERROR "QA-006 output omitted packaged scene/bundle")
endif()
if(NOT WIN32)
  execute_process(COMMAND "${destination}/NEnginePlayer" "${destination}"
                  RESULT_VARIABLE launched
                  OUTPUT_VARIABLE launch_output ERROR_VARIABLE launch_error)
  if(NOT launched EQUAL 0 OR NOT launch_output MATCHES "Player scene loaded")
    message(FATAL_ERROR "QA-006 portable Player bootstrap failed: ${launch_error}")
  endif()
else()
  if(NOT EXISTS "${destination}/NEnginePlayer.exe")
    message(FATAL_ERROR "QA-006 Windows Player binary missing from Unicode bundle")
  endif()
endif()
message(STATUS "QA-006 unicode/spaces/long project create-export-load PASS")
