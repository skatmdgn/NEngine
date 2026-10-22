#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <unordered_map>
#include "nengine/core/world.hpp"
#include "nengine/navigation/nav_mesh.hpp"
namespace nengine::navigation {
struct NavSurface {
 bool enabled{true};
 core::Vec3 size{1.f,0.f,1.f};
 std::uint32_t area_mask{1u};
 float traversal_cost{1.f};
};
struct NavAgentComponent {
 bool enabled{true};
 bool has_destination{false};
 core::Vec3 destination{};
 float speed{3.5f};
 float stopping_distance{0.1f};
 float radius{0.f};
 std::uint32_t area_mask{0xffffffffu};
};
inline core::ComponentTypeId nav_surface_type() noexcept {
 return core::ComponentRegistry::stable_id("NEngine.NavSurface");
}
inline core::ComponentTypeId nav_agent_type() noexcept {
 return core::ComponentRegistry::stable_id("NEngine.NavAgent");
}
struct NavWorldStats {
 std::size_t surfaces{0},connected{0},agents{0},moving{0},arrived{0},failed_paths{0};
};
class NavWorldSystem {
public:
 NavMesh& mesh() noexcept { return mesh_; }
 const NavMesh& mesh() const noexcept { return mesh_; }
 bool bake(const core::World&,NavWorldStats* = nullptr,std::string* = nullptr);
 void set_mesh(NavMesh mesh) { mesh_=std::move(mesh);agents_.clear(); }
 NavWorldStats step(core::World&,float);
 void reset() noexcept { mesh_.clear();agents_.clear(); }
private:
 struct AgentState { NavAgent motion{};core::Vec3 target{};bool assigned{false};bool failed{false}; };
 NavMesh mesh_{};
 std::unordered_map<std::uint64_t,AgentState> agents_{};
};
bool register_navigation_components(core::ComponentRegistry&);
bool register_navigation_serializers(core::ComponentSerializationRegistry&);
} // namespace nengine::navigation
