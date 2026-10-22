#include "nengine/navigation/nav_bake.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

namespace nengine::navigation {
namespace {
using VertexKey = std::tuple<float, float, float>;
using EdgeKey = std::pair<VertexKey, VertexKey>;
using FaceKey = std::array<VertexKey, 3>;

bool fail(std::string* error, const char* why) {
    if (error) *error = why;
    return false;
}
VertexKey key(core::Vec3 p) {
    // Finite coordinates only, and normalize signed zero for deterministic
    // edge/face identity independent of the source representation.
    return {p.x == 0.f ? 0.f : p.x,
            p.y == 0.f ? 0.f : p.y,
            p.z == 0.f ? 0.f : p.z};
}
EdgeKey edge_key(core::Vec3 a, core::Vec3 b) {
    auto ka = key(a), kb = key(b);
    if (kb < ka) std::swap(ka, kb);
    return {ka, kb};
}
FaceKey face_key(const std::array<core::Vec3, 3>& v) {
    FaceKey result{key(v[0]), key(v[1]), key(v[2])};
    std::sort(result.begin(), result.end());
    return result;
}
bool valid_options(const NavBakeOptions& o) {
    return std::isfinite(o.max_slope_degrees) &&
           o.max_slope_degrees >= 0.f && o.max_slope_degrees <= 89.9f &&
           std::isfinite(o.max_height_span) &&
           o.max_height_span >= 0.f && o.max_height_span <= 0.25f &&
           std::isfinite(o.min_projected_area) &&
           o.min_projected_area >= 0.f && o.min_projected_area <= 1000000.f &&
           std::isfinite(o.weld_tolerance) &&
           o.weld_tolerance >= 0.f && o.weld_tolerance <= 0.01f &&
           o.allowed_area_mask != 0u &&
           o.max_input_triangles > 0u &&
           o.max_input_triangles <= NavMesh::max_polygons;
}
core::Vec3 weld(core::Vec3 p, float tolerance) {
    if (tolerance == 0.f) return p;
    const auto snap = [tolerance](float value) {
        return static_cast<float>(
            std::round(static_cast<double>(value) / tolerance) * tolerance);
    };
    return {snap(p.x), snap(p.y), snap(p.z)};
}
} // namespace

bool bake_nav_triangles(std::span<const NavBakeTriangle> triangles,
                        NavMesh& output, const NavBakeOptions& options,
                        NavBakeReport* report, std::string* error) {
    if (!valid_options(options))
        return fail(error, "invalid navigation bake options");
    if (triangles.size() > options.max_input_triangles)
        return fail(error, "navigation triangle input exceeds budget");

    NavMesh staged;
    NavBakeReport staged_report;
    staged_report.input_triangles = triangles.size();
    std::set<FaceKey> faces;

    for (const auto& triangle : triangles) {
        if (triangle.area_mask == 0u ||
            !std::isfinite(triangle.traversal_cost) ||
            triangle.traversal_cost < 1.f ||
            triangle.traversal_cost > 100000.f)
            return fail(error, "invalid navigation triangle attributes");

        auto v = triangle.vertices;
        for (auto& p : v) {
            if (!finite_position(p))
                return fail(error, "invalid navigation triangle vertex");
            p = weld(p, options.weld_tolerance);
            if (!finite_position(p))
                return fail(error, "welded navigation vertex out of bounds");
        }
        const auto abx = static_cast<double>(v[1].x) - v[0].x;
        const auto aby = static_cast<double>(v[1].y) - v[0].y;
        const auto abz = static_cast<double>(v[1].z) - v[0].z;
        const auto acx = static_cast<double>(v[2].x) - v[0].x;
        const auto acy = static_cast<double>(v[2].y) - v[0].y;
        const auto acz = static_cast<double>(v[2].z) - v[0].z;
        const double nx = aby * acz - abz * acy;
        const double ny = abz * acx - abx * acz;
        const double nz = abx * acy - aby * acx;
        const double norm = std::hypot(std::hypot(nx, ny), nz);
        if (!std::isfinite(norm) || norm <= 1e-12)
            return fail(error, "degenerate navigation triangle");

        if ((triangle.area_mask & options.allowed_area_mask) == 0u) {
            ++staged_report.filtered_area;
            continue;
        }
        if (std::abs(ny) * 0.5 < options.min_projected_area ||
            std::abs(ny) <= 1e-12) {
            ++staged_report.filtered_small_area;
            continue;
        }
        const double slope = std::atan2(std::hypot(nx, nz), std::abs(ny)) *
                             (180.0 / 3.14159265358979323846);
        if (slope > static_cast<double>(options.max_slope_degrees) + 1e-7) {
            ++staged_report.filtered_slope;
            continue;
        }
        const float low = std::min({v[0].y, v[1].y, v[2].y});
        const float high = std::max({v[0].y, v[1].y, v[2].y});
        if (static_cast<double>(high) - low >
            static_cast<double>(options.max_height_span) + 1e-7) {
            ++staged_report.filtered_height;
            continue;
        }
        // Normalize winding: NavMesh::connect expects opposite directed
        // shared edges, regardless of input mesh's triangle winding.
        if (ny > 0.0) std::swap(v[1], v[2]);
        if (!faces.insert(face_key(v)).second)
            return fail(error, "duplicate navigation triangle");
        if (!staged.add_polygon({v[0], v[1], v[2]}, nullptr,
                                triangle.area_mask, triangle.traversal_cost))
            return fail(error, "navigation polygon bake rejected geometry");
        ++staged_report.accepted_triangles;
    }

    // Each edge may have at most two incident triangles. Reject non-manifold
    // geometry rather than inventing ambiguous path connections.
    std::map<EdgeKey, std::vector<NavId>> incidents;
    for (const auto& polygon : staged.polygons()) {
        const auto& v = polygon.vertices;
        for (std::size_t i = 0; i < 3u; ++i) {
            auto& neighbors = incidents[edge_key(v[i], v[(i + 1u) % 3u])];
            if (neighbors.size() >= 2u)
                return fail(error, "non-manifold navigation triangle edge");
            if (!neighbors.empty() && options.connect_shared_edges) {
                if (!staged.connect(neighbors[0], polygon.id))
                    return fail(error, "inconsistent navigation seam");
                ++staged_report.connected_edges;
            }
            neighbors.push_back(polygon.id);
        }
    }
    output = std::move(staged);
    if (report) *report = staged_report;
    if (error) error->clear();
    return true;
}

bool bake_indexed_nav_mesh(std::span<const core::Vec3> vertices,
                           std::span<const std::uint32_t> indices,
                           NavMesh& output, const NavBakeOptions& options,
                           std::uint32_t area_mask, float traversal_cost,
                           NavBakeReport* report, std::string* error) {
    if (!valid_options(options))
        return fail(error, "invalid navigation bake options");
    if (indices.size() % 3u != 0u)
        return fail(error, "navigation triangle indices not a multiple of three");
    if (indices.size() / 3u > options.max_input_triangles)
        return fail(error, "navigation indexed triangles exceed budget");
    // Build in a bounded scratch buffer, never mutate the destination if
    // any index is invalid (even if earlier triangles were valid).
    std::vector<NavBakeTriangle> triangles;
    triangles.reserve(indices.size() / 3u);
    for (std::size_t i = 0; i < indices.size(); i += 3u) {
        if (indices[i] >= vertices.size() ||
            indices[i + 1u] >= vertices.size() ||
            indices[i + 2u] >= vertices.size())
            return fail(error, "navigation triangle index out of range");
        triangles.push_back({{vertices[indices[i]], vertices[indices[i+1u]],
                              vertices[indices[i+2u]]}, area_mask,
                             traversal_cost});
    }
    return bake_nav_triangles(triangles, output, options, report, error);
}
} // namespace nengine::navigation
