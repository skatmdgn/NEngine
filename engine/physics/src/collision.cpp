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

CollisionResolutionStats resolve_box_contacts_3d(
    core::World& world,
    const std::vector<BoxOverlap>& overlaps) {

    CollisionResolutionStats stats;

    const auto dot =
        [](core::Vec3 a,
           core::Vec3 b) noexcept {
            return a.x * b.x +
                a.y * b.y +
                a.z * b.z;
        };

    const auto subtract_normal_velocity =
        [&dot](
            core::Vec3& velocity,
            core::Vec3 normal,
            bool first_body) {

            const float along =
                dot(
                    velocity,
                    normal);

            const bool entering =
                first_body
                    ? along > 0.0f
                    : along < 0.0f;

            if (!entering) {
                return;
            }

            velocity.x -=
                normal.x * along;
            velocity.y -=
                normal.y * along;
            velocity.z -=
                normal.z * along;
        };

    for (const auto& overlap :
         overlaps) {

        if (overlap.is_2d ||
            overlap.is_trigger ||
            overlap.penetration <= 0.0f) {
            continue;
        }

        auto* first_body =
            world.get_component<Rigidbody>(
                overlap.first,
                rigidbody_type());

        auto* second_body =
            world.get_component<Rigidbody>(
                overlap.second,
                rigidbody_type());

        const bool first_dynamic =
            first_body &&
            first_body->enabled &&
            !first_body->is_kinematic;

        const bool second_dynamic =
            second_body &&
            second_body->enabled &&
            !second_body->is_kinematic;

        if (!first_dynamic &&
            !second_dynamic) {
            continue;
        }

        auto* first_transform =
            world.transform(
                overlap.first);

        auto* second_transform =
            world.transform(
                overlap.second);

        if (!first_transform ||
            !second_transform) {
            continue;
        }

        float first_share = 0.0f;
        float second_share = 0.0f;

        if (first_dynamic &&
            second_dynamic) {

            const float first_inverse_mass =
                first_body->mass > 0.0f
                    ? 1.0f / first_body->mass
                    : 0.0f;

            const float second_inverse_mass =
                second_body->mass > 0.0f
                    ? 1.0f / second_body->mass
                    : 0.0f;

            const float total =
                first_inverse_mass +
                second_inverse_mass;

            if (total > 0.0f) {
                first_share =
                    first_inverse_mass /
                    total;
                second_share =
                    second_inverse_mass /
                    total;
            }
        } else if (first_dynamic) {
            first_share = 1.0f;
        } else {
            second_share = 1.0f;
        }

        first_transform->local_position.x -=
            overlap.normal.x *
            overlap.penetration *
            first_share;
        first_transform->local_position.y -=
            overlap.normal.y *
            overlap.penetration *
            first_share;
        first_transform->local_position.z -=
            overlap.normal.z *
            overlap.penetration *
            first_share;

        second_transform->local_position.x +=
            overlap.normal.x *
            overlap.penetration *
            second_share;
        second_transform->local_position.y +=
            overlap.normal.y *
            overlap.penetration *
            second_share;
        second_transform->local_position.z +=
            overlap.normal.z *
            overlap.penetration *
            second_share;

        if (first_dynamic) {
            subtract_normal_velocity(
                first_body->linear_velocity,
                overlap.normal,
                true);
        }

        if (second_dynamic) {
            subtract_normal_velocity(
                second_body->linear_velocity,
                overlap.normal,
                false);
        }

        ++stats.resolved_3d;
    }

    return stats;
}

CollisionResolutionStats resolve_box_contacts_2d(
    core::World& world,
    const std::vector<BoxOverlap>& overlaps) {

    CollisionResolutionStats stats;

    const auto dot2 =
        [](core::Vec3 a,
           core::Vec3 b) noexcept {
            return a.x * b.x +
                a.y * b.y;
        };

    const auto remove_normal_velocity =
        [&dot2](
            core::Vec3& velocity,
            core::Vec3 normal,
            bool first_body) {

            const float along =
                dot2(
                    velocity,
                    normal);

            const bool entering =
                first_body
                    ? along > 0.0f
                    : along < 0.0f;

            if (entering) {
                velocity.x -=
                    normal.x * along;
                velocity.y -=
                    normal.y * along;
            }

            velocity.z = 0.0f;
        };

    for (const auto& overlap :
         overlaps) {

        if (!overlap.is_2d ||
            overlap.is_trigger ||
            overlap.penetration <= 0.0f) {
            continue;
        }

        auto* first_body =
            world.get_component<Rigidbody2D>(
                overlap.first,
                rigidbody2d_type());

        auto* second_body =
            world.get_component<Rigidbody2D>(
                overlap.second,
                rigidbody2d_type());

        const bool first_dynamic =
            first_body &&
            first_body->enabled &&
            !first_body->is_kinematic;

        const bool second_dynamic =
            second_body &&
            second_body->enabled &&
            !second_body->is_kinematic;

        if (!first_dynamic &&
            !second_dynamic) {
            continue;
        }

        auto* first_transform =
            world.transform(
                overlap.first);

        auto* second_transform =
            world.transform(
                overlap.second);

        if (!first_transform ||
            !second_transform) {
            continue;
        }

        float first_share = 0.0f;
        float second_share = 0.0f;

        if (first_dynamic &&
            second_dynamic) {

            const float first_inverse_mass =
                first_body->mass > 0.0f
                    ? 1.0f / first_body->mass
                    : 0.0f;

            const float second_inverse_mass =
                second_body->mass > 0.0f
                    ? 1.0f / second_body->mass
                    : 0.0f;

            const float total =
                first_inverse_mass +
                second_inverse_mass;

            if (total > 0.0f) {
                first_share =
                    first_inverse_mass /
                    total;
                second_share =
                    second_inverse_mass /
                    total;
            }
        } else if (first_dynamic) {
            first_share = 1.0f;
        } else {
            second_share = 1.0f;
        }

        first_transform->local_position.x -=
            overlap.normal.x *
            overlap.penetration *
            first_share;
        first_transform->local_position.y -=
            overlap.normal.y *
            overlap.penetration *
            first_share;

        second_transform->local_position.x +=
            overlap.normal.x *
            overlap.penetration *
            second_share;
        second_transform->local_position.y +=
            overlap.normal.y *
            overlap.penetration *
            second_share;

        if (first_dynamic) {
            remove_normal_velocity(
                first_body->linear_velocity,
                overlap.normal,
                true);
        }

        if (second_dynamic) {
            remove_normal_velocity(
                second_body->linear_velocity,
                overlap.normal,
                false);
        }

        ++stats.resolved_2d;
    }

    return stats;
}

} // namespace nengine::physics
