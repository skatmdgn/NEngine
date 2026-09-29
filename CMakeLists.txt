cmake_minimum_required(VERSION 3.25)
project(NEngine VERSION 0.1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(NENGINE_BUILD_TESTS "Build NEngine tests" ON)
option(NENGINE_BUILD_EDITOR "Build the editor bootstrap" ON)

add_subdirectory(engine/core)
add_subdirectory(engine/editor)

if(NENGINE_BUILD_EDITOR)
  add_subdirectory(apps/editor)
endif()

if(NENGINE_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
