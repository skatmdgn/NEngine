#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>
#include "nengine/navigation/nav_mesh.hpp"

namespace nengine::navigation {

// A deliberately narrow, deterministic navigation surface cooker. Input is
// already in scene/world space. This is NOT a scene mesh extractor or an
// automatic obstacle/clearance baker.
struct NavBakeTriangle {
    std::array<core::Vec3, 3> vertices{};
    std::uint32_t area_mask{1u};
    float traversal_cost{1.0f};
};

struct NavBakeObstacle {
    core::Vec3 center{};
    float radius{0.5f};
};

struct NavBakeOptions {
    float max_slope_degrees{45.0f};
    float max_height_span{0.25f};
    float min_projected_area{0.0001f};
    float weld_tolerance{0.0f};
    std::uint32_t allowed_area_mask{0xffffffffu};
    std::size_t max_input_triangles{NavMesh::max_polygons};
    bool connect_shared_edges{true};
    // Split a triangle edge when another face terminates at its midpoint.
    // Disabled by default for exact compatibility with authored triangle IDs.
    bool resolve_t_junctions{false};
    // Static bake clearance: conservative polygon exclusion + portal width.
    float agent_radius{0.0f};
    std::vector<NavBakeObstacle> obstacles{};
};

struct NavBakeReport {
    std::size_t input_triangles{0};
    std::size_t accepted_triangles{0};
    std::size_t filtered_area{0};
    std::size_t filtered_slope{0};
    std::size_t filtered_height{0};
    std::size_t filtered_small_area{0};
    std::size_t connected_edges{0};
    std::size_t filtered_clearance{0};
};

// Transactional: output/report are updated only after complete validation.
// Geometry after cooking consists of flat-or-nearly-flat convex triangles;
// adjacency requires full matching edges; optional T-junction splitting
// constructs compatible boundary segments before publishing the graph.
bool bake_nav_triangles(std::span<const NavBakeTriangle> triangles,
                        NavMesh& output,
                        const NavBakeOptions& options = {},
                        NavBakeReport* report = nullptr,
                        std::string* error = nullptr);

// Convenience entry point for conventional indexed triangle-list meshes.
// Indices are triplets, vertex positions are world-space, and attributes are
// uniform across this mesh. Use bake_nav_triangles for per-face attributes.
bool bake_indexed_nav_mesh(std::span<const core::Vec3> vertices,
                           std::span<const std::uint32_t> indices,
                           NavMesh& output,
                           const NavBakeOptions& options = {},
                           std::uint32_t area_mask = 1u,
                           float traversal_cost = 1.0f,
                           NavBakeReport* report = nullptr,
                           std::string* error = nullptr);

} // namespace nengine::navigation
