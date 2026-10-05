#include "nengine/render/vulkan_builtin_mesh_cache.hpp"

#include "nengine/render/builtin_assets.hpp"
#include "nengine/render/mesh_data.hpp"

namespace nengine::render {

VulkanBuiltinMeshCache::~VulkanBuiltinMeshCache() {
    shutdown();
}

bool VulkanBuiltinMeshCache::initialize(
    const VulkanLoader& loader,
    const VulkanInstance& instance,
    const VulkanDevice& device) {

    shutdown();
    diagnostic_.clear();

    const auto cube =
        builtin_mesh_data(
            builtin_unit_cube_mesh_guid());

    const auto quad =
        builtin_mesh_data(
            builtin_unit_quad_mesh_guid());

    if (!cube || !quad) {
        diagnostic_ =
            "built-in CPU mesh definitions are unavailable";
        return false;
    }

    if (!cube_.create(
            loader,
            instance,
            device,
            *cube)) {

        diagnostic_ =
            "built-in cube GPU upload failed: " +
            cube_.diagnostic();

        shutdown();
        return false;
    }

    if (!quad_.create(
            loader,
            instance,
            device,
            *quad)) {

        diagnostic_ =
            "built-in quad GPU upload failed: " +
            quad_.diagnostic();

        shutdown();
        return false;
    }

    diagnostic_ =
        "Vulkan built-in mesh cache ready";

    return true;
}

void VulkanBuiltinMeshCache::shutdown() noexcept {
    quad_.destroy();
    cube_.destroy();
}

const VulkanMeshResource*
VulkanBuiltinMeshCache::find(
    assets::AssetGuid guid) const noexcept {

    if (guid ==
        builtin_unit_cube_mesh_guid()) {
        return cube_.valid()
            ? &cube_
            : nullptr;
    }

    if (guid ==
        builtin_unit_quad_mesh_guid()) {
        return quad_.valid()
            ? &quad_
            : nullptr;
    }

    return nullptr;
}

} // namespace nengine::render
