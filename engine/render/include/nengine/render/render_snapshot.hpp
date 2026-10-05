#pragma once

#include <vector>

#include "nengine/core/entity.hpp"
#include "nengine/core/transform.hpp"
#include "nengine/core/world.hpp"
#include "nengine/render/components.hpp"

namespace nengine::render {

struct CameraItem {
    core::Entity entity{core::Entity::invalid()};
    core::Transform transform{};
    Camera camera{};
};

struct LightItem {
    core::Entity entity{core::Entity::invalid()};
    core::Transform transform{};
    Light light{};
};

struct MeshItem {
    core::Entity entity{core::Entity::invalid()};
    core::Transform transform{};
    MeshRenderer renderer{};
};

struct RenderSnapshot {
    std::vector<CameraItem> cameras{};
    std::vector<LightItem> lights{};
    std::vector<MeshItem> meshes{};
};

RenderSnapshot build_render_snapshot(
    const core::World& world);

} // namespace nengine::render
