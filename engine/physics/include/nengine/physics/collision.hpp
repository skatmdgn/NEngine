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
};

struct CollisionDetectionResult {
    std::vector<BoxOverlap> overlaps{};
    std::size_t tested_pairs_3d{0};
    std::size_t tested_pairs_2d{0};
};

CollisionDetectionResult detect_box_overlaps(
    const core::World& world);

struct CollisionResolutionStats {
    std::size_t resolved_3d{0};
    std::size_t resolved_2d{0};
};

CollisionResolutionStats resolve_box_contacts_3d(
    core::World& world,
    const std::vector<BoxOverlap>& overlaps);

CollisionResolutionStats resolve_box_contacts_2d(
    core::World& world,
    const std::vector<BoxOverlap>& overlaps);

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

} // namespace nengine::physics
