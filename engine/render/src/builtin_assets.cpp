#include "nengine/render/builtin_assets.hpp"

namespace nengine::render {

bool is_builtin_mesh(
    assets::AssetGuid guid) noexcept {

    return
        guid ==
            builtin_unit_cube_mesh_guid() ||
        guid ==
            builtin_unit_quad_mesh_guid();
}

std::optional<MeshData>
builtin_mesh_data(
    assets::AssetGuid guid) {

    if (guid ==
        builtin_unit_cube_mesh_guid()) {
        return make_unit_cube_mesh();
    }

    if (guid ==
        builtin_unit_quad_mesh_guid()) {
        return make_unit_quad_mesh();
    }

    return std::nullopt;
}

} // namespace nengine::render
