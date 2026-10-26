add_library(NEngineNavigation STATIC
  src/nav_mesh.cpp
  src/nav_path.cpp
  src/nav_agent.cpp
  src/nav_serialization.cpp
  src/nav_bake.cpp
  src/nav_scene.cpp
  src/registration.cpp
  src/nav_scene_agent.cpp
)
target_include_directories(NEngineNavigation PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(NEngineNavigation PUBLIC NEngineCore NEngineRender)
target_compile_features(NEngineNavigation PUBLIC cxx_std_20)
if(MSVC)
  target_compile_options(NEngineNavigation PRIVATE /W4 /permissive-)
else()
  target_compile_options(NEngineNavigation PRIVATE -Wall -Wextra -Wpedantic)
endif()
