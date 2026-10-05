add_library(NEngineRender STATIC
  src/asset_resources.cpp
  src/matrix.cpp
  src/mesh_data.cpp
  src/registration.cpp
  src/render_snapshot.cpp
  src/vulkan_context.cpp
  src/vulkan_device.cpp
  src/vulkan_instance.cpp
  src/vulkan_presenter.cpp
  src/vulkan_swapchain.cpp
)

if(WIN32)
  target_sources(NEngineRender PRIVATE
    src/vulkan_loader_win32.cpp
    src/vulkan_surface_win32.cpp
  )
else()
  target_sources(NEngineRender PRIVATE
    src/vulkan_loader_posix.cpp
    src/vulkan_surface_stub.cpp
  )
  target_link_libraries(NEngineRender PUBLIC dl)
endif()

target_include_directories(NEngineRender
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(NEngineRender
  PUBLIC
    NEngineCore
    NEngineAssets
)

target_compile_features(NEngineRender PUBLIC cxx_std_20)

if(MSVC)
  target_compile_options(NEngineRender PRIVATE /W4 /permissive-)
else()
  target_compile_options(NEngineRender PRIVATE -Wall -Wextra -Wpedantic)
endif()
