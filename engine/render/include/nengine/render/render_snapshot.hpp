#pragma once

#include <vector>

#include "nengine/core/entity.hpp"
#include "nengine/core/transform.hpp"
#include "nengine/core/world.hpp"
#include "nengine/render/components.hpp"
#include "nengine/render/matrix.hpp"

namespace nengine::render {

struct CameraItem {
    core::Entity entity{core::Entity::invalid()};
    core::Transform transform{};
    Mat4 world{};
    Camera camera{};
};

struct LightItem {
    core::Entity entity{core::Entity::invalid()};
    core::Transform transform{};
    Mat4 world{};
    Light light{};
};

struct MeshItem {
    core::Entity entity{core::Entity::invalid()};
    core::Transform transform{};
    Mat4 world{};
    MeshRenderer renderer{};
};

struct SpriteItem {
    core::Entity entity{core::Entity::invalid()};
    core::Transform transform{};
    Mat4 world{};
    SpriteRenderer renderer{};
};

struct RenderSnapshot {
    std::vector<CameraItem> cameras{};
    std::vector<LightItem> lights{};
    std::vector<MeshItem> meshes{};
    std::vector<SpriteItem> sprites{};
};

RenderSnapshot build_render_snapshot(
    const core::World& world);

} // namespace nengine::render
