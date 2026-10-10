#pragma once

#include <cstdint>

#include "nengine/core/component_registry.hpp"
#include "nengine/core/math.hpp"

namespace nengine::physics {

struct Rigidbody {
    bool enabled{true};
    bool use_gravity{true};
    bool is_kinematic{false};
    float mass{1.0f};
    float gravity_scale{1.0f};
    core::Vec3 linear_velocity{};
};

struct BoxCollider {
    bool enabled{true};
    bool is_trigger{false};
    std::uint32_t layer{0};
    std::uint32_t collision_mask{0xffffffffu};
    core::Vec3 center{};
    core::Vec3 size{1.0f, 1.0f, 1.0f};
};

struct Rigidbody2D {
    bool enabled{true};
    bool use_gravity{true};
    bool is_kinematic{false};
    float mass{1.0f};
    float gravity_scale{1.0f};
    core::Vec3 linear_velocity{};
};

struct BoxCollider2D {
    bool enabled{true};
    bool is_trigger{false};
    std::uint32_t layer{0};
    std::uint32_t collision_mask{0xffffffffu};
    core::Vec3 center{};
    core::Vec3 size{1.0f, 1.0f, 0.0f};
};

inline core::ComponentTypeId rigidbody_type() noexcept {
    return core::ComponentRegistry::stable_id(
        "NEngine.Rigidbody");
}

inline core::ComponentTypeId box_collider_type() noexcept {
    return core::ComponentRegistry::stable_id(
        "NEngine.BoxCollider");
}

inline core::ComponentTypeId rigidbody2d_type() noexcept {
    return core::ComponentRegistry::stable_id(
        "NEngine.Rigidbody2D");
}

inline core::ComponentTypeId box_collider2d_type() noexcept {
    return core::ComponentRegistry::stable_id(
        "NEngine.BoxCollider2D");
}

} // namespace nengine::physics
