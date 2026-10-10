#pragma once

#include <cstddef>
#include <vector>

#include "nengine/core/entity.hpp"
#include "nengine/core/math.hpp"
#include "nengine/core/world.hpp"

namespace nengine::physics {

struct BoxOverlap {
    core::Entity first{core::Entity::invalid()};
    core::Entity second{core::Entity::invalid()};
    core::Vec3 normal{};
    float penetration{0.0f};
    bool is_trigger{false};
    bool is_2d{false};
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

} // namespace nengine::physics
