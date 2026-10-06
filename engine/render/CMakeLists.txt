add_library(NEngineRender STATIC
  src/asset_resources.cpp
  src/builtin_assets.cpp
  src/decoded_mesh.cpp
  src/decoded_texture.cpp
  src/diagnostic_shaders.cpp
  src/gltf_mesh.cpp
  src/material_asset.cpp
  src/matrix.cpp
  src/mesh_data.cpp
  src/registration.cpp
  src/render_snapshot.cpp
  src/stb_image_impl.cpp
  src/vulkan_buffer.cpp
  src/vulkan_builtin_mesh_cache.cpp
  src/vulkan_context.cpp
  src/vulkan_depth_target.cpp
  src/vulkan_device.cpp
  src/vulkan_diagnostic_scene.cpp
  src/vulkan_instance.cpp
  src/vulkan_material.cpp
  src/vulkan_material_asset_cache.cpp
  src/vulkan_mesh.cpp
  src/vulkan_mesh_asset_cache.cpp
  src/vulkan_pipeline.cpp
  src/vulkan_presenter.cpp
  src/vulkan_render_pass.cpp
  src/vulkan_render_targets.cpp
  src/vulkan_shader.cpp
  src/vulkan_swapchain.cpp
  src/vulkan_texture.cpp
  src/vulkan_texture_asset_cache.cpp
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
  PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/stb
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
