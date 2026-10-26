#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "nengine/core/math.hpp"
namespace nengine::navigation {
using NavId = std::uint32_t;
constexpr NavId invalid_nav_id = UINT32_MAX;
// Convex XZ polygons authored/baked elsewhere. No geometry auto-bake in this subset.
struct NavPolygon {
    NavId id{invalid_nav_id};
    std::vector<core::Vec3> vertices{};
    core::Vec3 center{};
    std::uint32_t area_mask{1u};
    float traversal_cost{1.0f};
    bool enabled{true};
};
struct NavLink {
    NavId id{invalid_nav_id};
    NavId from{invalid_nav_id};
    NavId to{invalid_nav_id};
    core::Vec3 enter{};
    core::Vec3 exit{};
    float cost{1.0f};
    bool bidirectional{false};
    bool enabled{true};
};
struct NavObstacle {
    NavId id{invalid_nav_id};
    core::Vec3 center{};
    float radius{0.5f};
    bool enabled{true};
};
struct NavQuery {
    std::uint32_t area_mask{0xffffffffu};
    float agent_radius{0.0f};
    bool allow_partial{false};
    std::size_t max_expansions{4096u};
    // Opt in to portal/funnel corner reduction; the authored centroid path
    // remains the compatibility default for existing games and tests.
    bool smooth_portals{false};
};
enum class NavPathStatus { Complete, Partial };
struct NavPath {
    NavPathStatus status{NavPathStatus::Complete};
    std::vector<NavId> polygons{};
    std::vector<core::Vec3> waypoints{};
    float cost{0.0f};
    std::size_t expansions{0};
    bool used_offmesh_link{false};
};
bool finite_position(core::Vec3 value) noexcept;
class NavMesh {
public:
    static constexpr std::size_t max_polygons = 4096u;
    static constexpr std::size_t max_vertices_per_polygon = 32u;
    static constexpr std::size_t max_links = 8192u;
    static constexpr std::size_t max_obstacles = 8192u;
    bool add_polygon(std::vector<core::Vec3> vertices,
                     NavId* out_id = nullptr, std::uint32_t area_mask = 1u,
                     float traversal_cost = 1.0f);
    bool set_polygon_enabled(NavId id, bool enabled) noexcept;
    bool set_polygon_area(NavId id, std::uint32_t mask, float cost) noexcept;
    const NavPolygon* polygon(NavId id) const noexcept;
    bool connect(NavId a, NavId b);
    bool add_link(NavId from, NavId to, core::Vec3 enter,
                  core::Vec3 exit, float cost, bool bidirectional,
                  NavId* out_id = nullptr);
    bool set_link_enabled(NavId id, bool enabled) noexcept;
    bool add_obstacle(core::Vec3 center, float radius, NavId* out_id = nullptr);
    bool remove_obstacle(NavId id) noexcept;
    bool set_obstacle_enabled(NavId id, bool enabled) noexcept;
    const std::vector<NavPolygon>& polygons() const noexcept { return polygons_; }
    const std::vector<std::pair<NavId, NavId>>& edges() const noexcept { return edges_; }
    const std::vector<NavLink>& links() const noexcept { return links_; }
    const std::vector<NavObstacle>& obstacles() const noexcept { return obstacles_; }
    bool contains(NavId polygon, core::Vec3 point) const noexcept;
    std::optional<NavId> locate(core::Vec3 point, const NavQuery& query = {}) const noexcept;
    std::optional<NavPath> find_path(core::Vec3 start, core::Vec3 goal,
                                     const NavQuery& query = {}) const;
    bool blocked(NavId id, float agent_radius = 0.0f) const noexcept;
    void clear() noexcept;
    std::uint64_t revision() const noexcept { return revision_; }
private:
    std::vector<NavPolygon> polygons_{};
    std::vector<std::pair<NavId, NavId>> edges_{};
    std::vector<NavLink> links_{};
    std::vector<NavObstacle> obstacles_{};
    std::uint64_t revision_{1};
};
struct NavAgent {
    core::Vec3 position{};
    core::Vec3 velocity{};
    float speed{3.5f};
    float stopping_distance{0.1f};
    bool enabled{true};
    bool arrived{true};
    std::vector<core::Vec3> route{};
    std::size_t next_waypoint{0};
    std::optional<core::Vec3> destination{};
    NavQuery navigation_query{};
    std::uint64_t planned_revision{0};
    bool set_destination(const NavMesh& mesh, core::Vec3 goal,
                         const NavQuery& query = {});
    bool update_navigation(const NavMesh& mesh, float delta_seconds);
    bool assign(const NavPath& path) noexcept;
    void stop() noexcept;
    bool update(float delta_seconds) noexcept;
};
bool write_nav_mesh(std::ostream& output, const NavMesh& mesh,
                    std::string* error = nullptr);
bool read_nav_mesh(std::istream& input, NavMesh& mesh,
                   std::string* error = nullptr);
bool save_nav_mesh(const std::filesystem::path& path, const NavMesh& mesh,
                   std::string* error = nullptr);
bool load_nav_mesh(const std::filesystem::path& path, NavMesh& mesh,
                   std::string* error = nullptr);
} // namespace nengine::navigation
