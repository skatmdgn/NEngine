cmake_minimum_required(VERSION 3.25)

if(POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW)
endif()

project(NEngine VERSION 0.2.7 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

option(NENGINE_BUILD_TESTS "Build NEngine tests" ON)
option(NENGINE_BUILD_EDITOR "Build the editor bootstrap" ON)

add_subdirectory(engine/core)
add_subdirectory(engine/platform)
add_subdirectory(engine/assets)
add_subdirectory(engine/render)
add_subdirectory(engine/scripting)
add_subdirectory(engine/editor)

if(NENGINE_BUILD_EDITOR)
  add_subdirectory(apps/editor)
endif()

if(NENGINE_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
