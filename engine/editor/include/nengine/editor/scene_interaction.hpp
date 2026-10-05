#pragma once

#include <cstdint>

#include "nengine/core/entity.hpp"
#include "nengine/core/math.hpp"
#include "nengine/core/world.hpp"

namespace nengine::editor {

struct SceneViewPoint {
    float x{0.0f};
    float y{0.0f};
};

struct SceneViewConfig {
    float pixels_per_unit{25.0f};
    float pick_radius_pixels{11.0f};
    float gizmo_axis_length_pixels{54.0f};
    float gizmo_axis_hit_pixels{7.0f};
};

enum class SceneGizmoAxis : std::uint8_t {
    None,
    X,
    Z,
};

core::Vec3 scene_world_position(const core::World& world, core::Entity entity);

SceneViewPoint scene_world_to_screen(
    const core::Vec3& world_position,
    float viewport_width,
    float viewport_height,
    const SceneViewConfig& config = {});

SceneViewPoint scene_entity_to_screen(
    const core::World& world,
    core::Entity entity,
    float viewport_width,
    float viewport_height,
    const SceneViewConfig& config = {});

core::Entity pick_scene_entity(
    const core::World& world,
    float screen_x,
    float screen_y,
    float viewport_width,
    float viewport_height,
    const SceneViewConfig& config = {});

SceneGizmoAxis hit_test_translate_gizmo(
    const core::World& world,
    core::Entity entity,
    float screen_x,
    float screen_y,
    float viewport_width,
    float viewport_height,
    const SceneViewConfig& config = {});

core::Vec3 translated_local_position_from_drag(
    const core::Vec3& original_local_position,
    SceneGizmoAxis axis,
    float screen_delta_x,
    float screen_delta_y,
    const SceneViewConfig& config = {});

} // namespace nengine::editor
