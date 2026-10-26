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

bool on_edge_interior(core::Vec3 p,core::Vec3 a,core::Vec3 b,double& t) {
    const double dx=static_cast<double>(b.x)-a.x;
    const double dy=static_cast<double>(b.y)-a.y;
    const double dz=static_cast<double>(b.z)-a.z;
    const double length2=dx*dx+dy*dy+dz*dz;
    if(length2<=1e-12)return false;
    t=((static_cast<double>(p.x)-a.x)*dx+
       (static_cast<double>(p.y)-a.y)*dy+
       (static_cast<double>(p.z)-a.z)*dz)/length2;
    if(t<=1e-5||t>=1.-1e-5)return false;
    const double ex=static_cast<double>(a.x)+t*dx-p.x;
    const double ey=static_cast<double>(a.y)+t*dy-p.y;
    const double ez=static_cast<double>(a.z)+t*dz-p.z;
    return ex*ex+ey*ey+ez*ez<=1e-10;
}
bool subdivide_t_junctions(std::span<const NavBakeTriangle> original,
                           std::size_t limit,
                           std::vector<NavBakeTriangle>& expanded,
                           std::string* error) {
    for(const auto& face:original){
        std::vector<core::Vec3> boundary;
        bool split=false;
        for(std::size_t edge=0;edge<3u;++edge){
            const auto a=face.vertices[edge];
            const auto b=face.vertices[(edge+1u)%3u];
            boundary.push_back(a);
            std::vector<std::pair<double,core::Vec3>> positions;
            for(const auto& other:original) {
                for(const auto point:other.vertices){
                    double t=0.;
                    if(on_edge_interior(point,a,b,t))positions.push_back({t,point});
                }
            }
            std::sort(positions.begin(),positions.end(),[](const auto& x,const auto& y){
                return x.first<y.first;
            });
            double last=-1.;
            for(const auto& [t,p]:positions){
                if(t-last<1e-5)continue;
                boundary.push_back(p);
                last=t;
                split=true;
            }
        }
        const std::size_t generated=split?boundary.size():1u;
        if(generated>limit||expanded.size()>limit-generated)
            return fail(error,"T-junction expansion exceeds navigation triangle budget");
        if(!split) {
            expanded.push_back(face);
            continue;
        }
        const auto& v=face.vertices;
        const core::Vec3 center{
            (v[0].x+v[1].x+v[2].x)/3.f,
            (v[0].y+v[1].y+v[2].y)/3.f,
            (v[0].z+v[1].z+v[2].z)/3.f
        };
        for(std::size_t i=0;i<boundary.size();++i)
            expanded.push_back({{center,boundary[i],
                boundary[(i+1u)%boundary.size()]},
                face.area_mask,face.traversal_cost});
    }
    return true;
}
} // namespace

bool bake_nav_triangles(std::span<const NavBakeTriangle> triangles,
                        NavMesh& output, const NavBakeOptions& options,
                        NavBakeReport* report, std::string* error) {
    if (!valid_options(options))
        return fail(error, "invalid navigation bake options");
    if (triangles.size() > options.max_input_triangles)
        return fail(error, "navigation triangle input exceeds budget");

    std::vector<NavBakeTriangle> expanded;
    std::span<const NavBakeTriangle> faces_to_bake=triangles;
    if(options.resolve_t_junctions) {
        if(!subdivide_t_junctions(triangles,options.max_input_triangles,
                                 expanded,error))return false;
        faces_to_bake=expanded;
    }
    NavMesh staged;
    NavBakeReport staged_report;
    staged_report.input_triangles = triangles.size();
    std::set<FaceKey> faces;

    for (const auto& triangle : faces_to_bake) {
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
