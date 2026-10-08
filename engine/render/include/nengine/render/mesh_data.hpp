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

inline constexpr std::uint32_t
    kMeshMaterialUnassigned =
        0xffffffffu;

struct MeshSubmesh {
    std::uint32_t first_index{0};
    std::uint32_t index_count{0};
    std::uint32_t material_slot{
        kMeshMaterialUnassigned};

    bool valid(
        std::size_t total_indices) const noexcept {

        return index_count != 0u &&
            (index_count % 3u) == 0u &&
            first_index <= total_indices &&
            index_count <=
                total_indices - first_index;
    }
};

struct MeshData {
    std::vector<MeshVertex> vertices{};
    std::vector<std::uint32_t> indices{};
    std::vector<MeshSubmesh> submeshes{};
    MeshBounds bounds{};

    bool valid() const noexcept {
        if (vertices.empty() ||
            indices.empty() ||
            (indices.size() % 3u) != 0u) {
            return false;
        }

        for (const auto& submesh :
             submeshes) {
            if (!submesh.valid(
                    indices.size())) {
                return false;
            }
        }

        return true;
    }
};

MeshData make_unit_cube_mesh();
MeshData make_unit_quad_mesh();

} // namespace nengine::render
