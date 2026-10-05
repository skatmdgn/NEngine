#pragma once

#include <optional>

#include "nengine/assets/asset_guid.hpp"
#include "nengine/render/mesh_data.hpp"

namespace nengine::render {

inline constexpr assets::AssetGuid
builtin_unit_cube_mesh_guid() noexcept {
    return {
        0x4E454E47494E4501ull,
        0x0000000000000001ull
    };
}

inline constexpr assets::AssetGuid
builtin_unit_quad_mesh_guid() noexcept {
    return {
        0x4E454E47494E4501ull,
        0x0000000000000002ull
    };
}

bool is_builtin_mesh(
    assets::AssetGuid guid) noexcept;

std::optional<MeshData>
builtin_mesh_data(
    assets::AssetGuid guid);

} // namespace nengine::render
