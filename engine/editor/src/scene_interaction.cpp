#include "nengine/editor/scene_interaction.hpp"

#include <cmath>

namespace nengine::editor {
namespace {

float square(float value) noexcept {
    return value * value;
}

} // namespace

core::Vec3 scene_world_position(
    const core::World& world,
    core::Entity entity) {

    core::Vec3 result{};
    auto current = entity;
    std::size_t remaining = world.size() + 1u;

    while (world.is_alive(current) && remaining-- > 0u) {
        const auto* transform = world.transform(current);
        if (!transform) break;

        result.x += transform->local_position.x;
        result.y += transform->local_position.y;
        result.z += transform->local_position.z;
        current = transform->parent;
    }

    return result;
}

SceneViewPoint scene_world_to_screen(
    const core::Vec3& world_position,
    float viewport_width,
    float viewport_height,
    const SceneViewConfig& config) {

    return {
        viewport_width * 0.5f + world_position.x * config.pixels_per_unit,
        viewport_height * 0.5f - world_position.z * config.pixels_per_unit,
    };
}

SceneViewPoint scene_entity_to_screen(
    const core::World& world,
    core::Entity entity,
    float viewport_width,
    float viewport_height,
    const SceneViewConfig& config) {

    return scene_world_to_screen(
        scene_world_position(world, entity),
        viewport_width,
        viewport_height,
        config);
}

core::Entity pick_scene_entity(
    const core::World& world,
    float screen_x,
    float screen_y,
    float viewport_width,
    float viewport_height,
    const SceneViewConfig& config) {

    core::Entity best = core::Entity::invalid();
    float best_distance = square(config.pick_radius_pixels);

    for (const auto entity : world.entities()) {
        if (!world.active(entity)) continue;

        const auto point = scene_entity_to_screen(
            world, entity, viewport_width, viewport_height, config);

        const float distance =
            square(point.x - screen_x) + square(point.y - screen_y);

        if (distance <= best_distance) {
            best_distance = distance;
            best = entity;
        }
    }

    return best;
}

SceneGizmoAxis hit_test_translate_gizmo(
    const core::World& world,
    core::Entity entity,
    float screen_x,
    float screen_y,
    float viewport_width,
    float viewport_height,
    const SceneViewConfig& config) {

    if (!world.is_alive(entity)) return SceneGizmoAxis::None;

    const auto origin = scene_entity_to_screen(
        world, entity, viewport_width, viewport_height, config);

    constexpr float start_offset = 8.0f;
    const float end_offset = config.gizmo_axis_length_pixels;

    const bool in_x_range =
        screen_x >= origin.x + start_offset &&
        screen_x <= origin.x + end_offset;

    const bool in_z_range =
        screen_y <= origin.y - start_offset &&
        screen_y >= origin.y - end_offset;

    const float x_distance = std::fabs(screen_y - origin.y);
    const float z_distance = std::fabs(screen_x - origin.x);

    const bool hit_x =
        in_x_range && x_distance <= config.gizmo_axis_hit_pixels;

    const bool hit_z =
        in_z_range && z_distance <= config.gizmo_axis_hit_pixels;

    if (hit_x && hit_z) {
        return x_distance <= z_distance
            ? SceneGizmoAxis::X
            : SceneGizmoAxis::Z;
    }

    if (hit_x) return SceneGizmoAxis::X;
    if (hit_z) return SceneGizmoAxis::Z;
    return SceneGizmoAxis::None;
}

core::Vec3 translated_local_position_from_drag(
    const core::Vec3& original_local_position,
    SceneGizmoAxis axis,
    float screen_delta_x,
    float screen_delta_y,
    const SceneViewConfig& config) {

    auto result = original_local_position;
    const float scale =
        config.pixels_per_unit > 0.0f ? config.pixels_per_unit : 1.0f;

    switch (axis) {
    case SceneGizmoAxis::X:
        result.x += screen_delta_x / scale;
        break;
    case SceneGizmoAxis::Z:
        result.z -= screen_delta_y / scale;
        break;
    case SceneGizmoAxis::None:
        break;
    }

    return result;
}

} // namespace nengine::editor
