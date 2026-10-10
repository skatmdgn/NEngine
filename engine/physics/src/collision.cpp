#include "nengine/physics/collision.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include "nengine/physics/components.hpp"

namespace nengine::physics {
namespace {

struct ColliderBounds {
    core::Entity entity{
        core::Entity::invalid()};
    core::Vec3 center{};
    core::Vec3 half{};
    bool trigger{false};
};

float axis_sign(
    float delta) noexcept {

    return delta >= 0.0f
        ? 1.0f
        : -1.0f;
}

core::Vec3 abs_scale(
    core::Vec3 value) noexcept {

    return {
        std::abs(value.x),
        std::abs(value.y),
        std::abs(value.z)
    };
}

template <typename Collider>
ColliderBounds make_bounds(
    const core::World& world,
    core::Entity entity,
    const Collider& collider,
    bool is_2d) {

    ColliderBounds result;
    result.entity = entity;
    result.trigger =
        collider.is_trigger;

    const auto* transform =
        world.transform(entity);

    if (!transform) {
        return result;
    }

    const auto scale =
        abs_scale(
            transform->local_scale);

    result.center = {
        transform->local_position.x +
            collider.center.x * scale.x,
        transform->local_position.y +
            collider.center.y * scale.y,
        transform->local_position.z +
            collider.center.z * scale.z
    };

    result.half = {
        std::abs(collider.size.x) *
            scale.x * 0.5f,
        std::abs(collider.size.y) *
            scale.y * 0.5f,
        is_2d
            ? 0.0f
            : std::abs(collider.size.z) *
                scale.z * 0.5f
    };

    return result;
}

bool overlap_pair(
    const ColliderBounds& a,
    const ColliderBounds& b,
    bool is_2d,
    BoxOverlap& overlap) noexcept {

    const auto delta = core::Vec3{
        b.center.x - a.center.x,
        b.center.y - a.center.y,
        b.center.z - a.center.z
    };

    const float px =
        a.half.x +
        b.half.x -
        std::abs(delta.x);

    const float py =
        a.half.y +
        b.half.y -
        std::abs(delta.y);

    if (px <= 0.0f ||
        py <= 0.0f) {
        return false;
    }

    float penetration = px;
    core::Vec3 normal{
        axis_sign(delta.x),
        0.0f,
        0.0f};

    if (py < penetration) {
        penetration = py;
        normal = {
            0.0f,
            axis_sign(delta.y),
            0.0f};
    }

    if (!is_2d) {
        const float pz =
            a.half.z +
            b.half.z -
            std::abs(delta.z);

        if (pz <= 0.0f) {
            return false;
        }

        if (pz < penetration) {
            penetration = pz;
            normal = {
                0.0f,
                0.0f,
                axis_sign(delta.z)};
        }
    }

    overlap = {
        a.entity,
        b.entity,
        normal,
        penetration,
        a.trigger || b.trigger,
        is_2d
    };

    return true;
}

template <typename Collider>
std::vector<ColliderBounds> collect_bounds(
    const core::World& world,
    core::ComponentTypeId type,
    bool is_2d) {

    std::vector<ColliderBounds> result;

    for (const auto entity :
         world.entities()) {

        if (!world.active(entity)) {
            continue;
        }

        const auto* collider =
            world.get_component<Collider>(
                entity,
                type);

        if (!collider ||
            !collider->enabled) {
            continue;
        }

        result.push_back(
            make_bounds(
                world,
                entity,
                *collider,
                is_2d));
    }

    return result;
}

template <typename Collider>
void append_overlaps(
    const core::World& world,
    core::ComponentTypeId type,
    bool is_2d,
    std::vector<BoxOverlap>& overlaps,
    std::size_t& tested_pairs) {

    const auto bounds =
        collect_bounds<Collider>(
            world,
            type,
            is_2d);

    for (std::size_t i = 0;
         i < bounds.size();
         ++i) {
        for (std::size_t j = i + 1;
             j < bounds.size();
             ++j) {

            ++tested_pairs;

            BoxOverlap overlap;

            if (overlap_pair(
                    bounds[i],
                    bounds[j],
                    is_2d,
                    overlap)) {
                overlaps.push_back(
                    overlap);
            }
        }
    }
}

} // namespace

CollisionDetectionResult detect_box_overlaps(
    const core::World& world) {

    CollisionDetectionResult result;

    append_overlaps<BoxCollider>(
        world,
        box_collider_type(),
        false,
        result.overlaps,
        result.tested_pairs_3d);

    append_overlaps<BoxCollider2D>(
        world,
        box_collider2d_type(),
        true,
        result.overlaps,
        result.tested_pairs_2d);

    return result;
}

} // namespace nengine::physics
