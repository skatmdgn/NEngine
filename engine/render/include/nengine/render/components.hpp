#pragma once

#include <cstdint>

#include "nengine/assets/asset_guid.hpp"
#include "nengine/core/component_registry.hpp"
#include "nengine/core/math.hpp"

namespace nengine::render {

enum class ProjectionMode : std::uint8_t {
    Perspective,
    Orthographic,
};

struct Camera {
    bool enabled{true};
    ProjectionMode projection{ProjectionMode::Perspective};
    float vertical_fov_degrees{60.0f};
    float near_clip{0.1f};
    float far_clip{1000.0f};
    float orthographic_size{5.0f};
};

enum class LightType : std::uint8_t {
    Directional,
    Point,
    Spot,
};

struct Light {
    bool enabled{true};
    LightType type{LightType::Directional};
    core::Vec3 color{1.0f, 1.0f, 1.0f};
    float intensity{1.0f};
    float range{10.0f};
    float spot_angle_degrees{30.0f};
    bool cast_shadows{true};
};

struct MeshRenderer {
    bool enabled{true};
    assets::AssetGuid mesh{};
    assets::AssetGuid material{};
    bool cast_shadows{true};
    bool receive_shadows{true};
};

inline core::ComponentTypeId camera_type() noexcept {
    return core::ComponentRegistry::stable_id("NEngine.Camera");
}

inline core::ComponentTypeId light_type() noexcept {
    return core::ComponentRegistry::stable_id("NEngine.Light");
}

inline core::ComponentTypeId mesh_renderer_type() noexcept {
    return core::ComponentRegistry::stable_id("NEngine.MeshRenderer");
}

} // namespace nengine::render
