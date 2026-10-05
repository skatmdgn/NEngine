#pragma once

#include <cstdint>
#include <vector>

#include "nengine/core/math.hpp"

namespace nengine::render {

struct MeshVertex {
    core::Vec3 position{};
    core::Vec3 normal{};
    core::Vec2 uv{};
};

struct MeshBounds {
    core::Vec3 center{};
    core::Vec3 extents{};
};

struct MeshData {
    std::vector<MeshVertex> vertices{};
    std::vector<std::uint32_t> indices{};
    MeshBounds bounds{};

    bool valid() const noexcept {
        return !vertices.empty() &&
            !indices.empty() &&
            (indices.size() % 3u) == 0u;
    }
};

MeshData make_unit_cube_mesh();
MeshData make_unit_quad_mesh();

} // namespace nengine::render
