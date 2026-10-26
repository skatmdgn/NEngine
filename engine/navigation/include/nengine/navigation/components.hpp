#pragma once
#include "nengine/core/component_registry.hpp"
#include "nengine/navigation/nav_mesh.hpp"
namespace nengine::navigation {
struct NavAgentComponent {
    bool enabled{true};
    bool has_destination{false};
    core::Vec3 target{};
    float speed{3.5f};
    float stopping_distance{0.1f};
    float radius{0.0f};
    NavAgent runner{}; // runtime-only, never serialized
    bool runtime_initialized{false};
};
inline core::ComponentTypeId nav_agent_type() noexcept {
    return core::ComponentRegistry::stable_id("NEngine.NavMeshAgent");
}
}