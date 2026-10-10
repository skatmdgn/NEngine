#pragma once

#include <cstdint>

#include "nengine/core/component_registry.hpp"
#include "nengine/core/math.hpp"

namespace nengine::physics {

struct Rigidbody {
    bool enabled{true};
    bool use_gravity{true};
    bool is_kinematic{false};
    bool allow_sleep{true};
    bool sleeping{false};
    float mass{1.0f};
    float gravity_scale{1.0f};
    float sleep_threshold{0.05f};
    float sleep_timer{0.0f};
    core::Vec3 linear_velocity{};
    core::Vec3 angular_velocity{};
};

struct BoxCollider {
    bool enabled{true};
    bool is_trigger{false};
    std::uint32_t layer{0};
    std::uint32_t collision_mask{0xffffffffu};
    float friction{0.5f};
    float restitution{0.0f};
    core::Vec3 center{};
    core::Vec3 size{1.0f, 1.0f, 1.0f};
};

struct SphereCollider {
    bool enabled{true};
    bool is_trigger{false};
    std::uint32_t layer{0};
    std::uint32_t collision_mask{0xffffffffu};
    float friction{0.5f};
    float restitution{0.0f};
    core::Vec3 center{};
    float radius{0.5f};
};

struct CapsuleCollider {
    bool enabled{true};
    bool is_trigger{false};
    std::uint32_t layer{0};
    std::uint32_t collision_mask{0xffffffffu};
    float friction{0.5f};
    float restitution{0.0f};
    core::Vec3 center{};
    float radius{0.5f};
    float height{2.0f};
    std::uint32_t direction{1};
};

struct Rigidbody2D {
    bool enabled{true};
    bool use_gravity{true};
    bool is_kinematic{false};
    bool allow_sleep{true};
    bool sleeping{false};
    float mass{1.0f};
    float gravity_scale{1.0f};
    float sleep_threshold{0.05f};
    float sleep_timer{0.0f};
    core::Vec3 linear_velocity{};
    core::Vec3 angular_velocity{};
};

struct BoxCollider2D {
    bool enabled{true};
    bool is_trigger{false};
    std::uint32_t layer{0};
    std::uint32_t collision_mask{0xffffffffu};
    float friction{0.5f};
    float restitution{0.0f};
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

inline core::ComponentTypeId sphere_collider_type() noexcept {
    return core::ComponentRegistry::stable_id(
        "NEngine.SphereCollider");
}

inline core::ComponentTypeId capsule_collider_type() noexcept {
    return core::ComponentRegistry::stable_id(
        "NEngine.CapsuleCollider");
}

inline core::ComponentTypeId rigidbody2d_type() noexcept {
    return core::ComponentRegistry::stable_id(
        "NEngine.Rigidbody2D");
}

inline core::ComponentTypeId box_collider2d_type() noexcept {
    return core::ComponentRegistry::stable_id(
        "NEngine.BoxCollider2D");
}

struct CircleCollider2D {
    bool enabled{true};
    bool is_trigger{false};
    std::uint32_t layer{0};
    std::uint32_t collision_mask{0xffffffffu};
    float friction{0.5f};
    float restitution{0.0f};
    core::Vec3 center{};
    float radius{0.5f};
};

struct CapsuleCollider2D {
    bool enabled{true};
    bool is_trigger{false};
    std::uint32_t layer{0};
    std::uint32_t collision_mask{0xffffffffu};
    float friction{0.5f};
    float restitution{0.0f};
    core::Vec3 center{};
    core::Vec3 size{1.0f, 2.0f, 0.0f};
    std::uint32_t direction{0};
};

inline core::ComponentTypeId circle_collider2d_type() noexcept {
    return core::ComponentRegistry::stable_id(
        "NEngine.CircleCollider2D");
}

inline core::ComponentTypeId capsule_collider2d_type() noexcept {
    return core::ComponentRegistry::stable_id(
        "NEngine.CapsuleCollider2D");
}

} // namespace nengine::physics
