#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "nengine/core/entity.hpp"
#include "nengine/core/math.hpp"
#include "nengine/core/world.hpp"

namespace nengine::physics {

struct ContactPoint {
    core::Vec3 point{};
    float penetration{0.0f};
};

struct ContactManifold {
    static constexpr std::size_t
        max_points = 4u;

    std::array<
        ContactPoint,
        max_points>
        points{};

    std::size_t count{0};
};

struct BoxOverlap {
    core::Entity first{core::Entity::invalid()};
    core::Entity second{core::Entity::invalid()};
    core::Vec3 normal{};
    float penetration{0.0f};
    bool is_trigger{false};
    bool is_2d{false};
    ContactManifold manifold{};
    // Shape slot: 0 Box, 1 Sphere/Circle, 2 Capsule.
    // Identity prevents different solid collider pairs sharing impulses.
    std::uint8_t first_shape{0u};
    std::uint8_t second_shape{0u};
};

struct CollisionDetectionResult {
    std::vector<BoxOverlap> overlaps{};
    std::size_t tested_pairs_3d{0};
    std::size_t tested_pairs_2d{0};
};

CollisionDetectionResult detect_box_overlaps(
    const core::World& world);

// Scene-owned cache; reset on scene/play-session changes.
struct CachedContactImpulse {
    core::Vec3 point{};
    float normal_impulse{0.0f};
    core::Vec3 tangent_impulse{};
};

struct CachedContactPair {
    core::Entity first{core::Entity::invalid()};
    core::Entity second{core::Entity::invalid()};
    core::Vec3 normal{};
    bool is_2d{false};
    std::array<CachedContactImpulse, 4> points{};
    std::size_t count{0};
    std::uint8_t first_shape{0u};
    std::uint8_t second_shape{0u};
};

struct ContactImpulseCache {
    std::vector<CachedContactPair> previous{};
    std::vector<CachedContactPair> current{};
    void begin_frame() { current.clear(); }
    void end_frame() { previous.swap(current); current.clear(); }
    void clear() { previous.clear(); current.clear(); }
};

struct CollisionResolutionStats {
    std::size_t resolved_3d{0};
    std::size_t resolved_2d{0};
    std::size_t warm_started_contacts{0};
};

CollisionResolutionStats resolve_box_contacts_3d(
    core::World& world,
    const std::vector<BoxOverlap>& overlaps,
    ContactImpulseCache* cache = nullptr);

CollisionResolutionStats resolve_box_contacts_2d(
    core::World& world,
    const std::vector<BoxOverlap>& overlaps,
    ContactImpulseCache* cache = nullptr);

std::vector<core::Entity> overlap_box(
    const core::World& world,
    core::Vec3 center,
    core::Vec3 size,
    bool include_triggers = true,
    std::uint32_t layer_mask = 0xffffffffu);

std::vector<core::Entity> overlap_box_2d(
    const core::World& world,
    core::Vec2 center,
    core::Vec2 size,
    bool include_triggers = true,
    std::uint32_t layer_mask = 0xffffffffu);

struct RaycastHit {
    core::Entity entity{
        core::Entity::invalid()};
    core::Vec3 point{};
    core::Vec3 normal{};
    float distance{0.0f};
    bool is_trigger{false};
    bool is_2d{false};
    std::uint32_t layer{0};
};

std::optional<RaycastHit> raycast(
    const core::World& world,
    core::Vec3 origin,
    core::Vec3 direction,
    float max_distance,
    bool include_triggers = true,
    std::uint32_t layer_mask = 0xffffffffu);

std::optional<RaycastHit> raycast_2d(
    const core::World& world,
    core::Vec2 origin,
    core::Vec2 direction,
    float max_distance,
    bool include_triggers = true,
    std::uint32_t layer_mask = 0xffffffffu);

std::optional<RaycastHit> box_cast(
    const core::World& world,
    core::Vec3 origin,
    core::Vec3 size,
    core::Vec3 direction,
    float max_distance,
    bool include_triggers = true,
    std::uint32_t layer_mask = 0xffffffffu);

std::optional<RaycastHit> box_cast_2d(
    const core::World& world,
    core::Vec2 origin,
    core::Vec2 size,
    core::Vec2 direction,
    float max_distance,
    bool include_triggers = true,
    std::uint32_t layer_mask = 0xffffffffu);


 // Continuous, geometry-aware world-space casts. Distance is measured along
 // normalized travel direction; capsule's long axis is world +Y.
 // Only enabled 3D colliders participate. The hit point is the cast origin
 // translated to time-of-impact, consistent with box_cast.
std::optional<RaycastHit> sphere_cast(
    const core::World& world,
    core::Vec3 origin,
    float radius,
    core::Vec3 direction,
    float max_distance,
    bool include_triggers = true,
    std::uint32_t layer_mask = 0xffffffffu);

std::optional<RaycastHit> capsule_cast(
    const core::World& world,
    core::Vec3 origin,
    float radius,
    float height,
    core::Vec3 direction,
    float max_distance,
    bool include_triggers = true,
    std::uint32_t layer_mask = 0xffffffffu);

} // namespace nengine::physics
