#include "nengine/physics/collision.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include "nengine/physics/components.hpp"

namespace nengine::physics {
namespace {

enum class ColliderShape {
    Box,
    Radial
};

struct ColliderBounds {
    core::Entity entity{
        core::Entity::invalid()};
    core::Vec3 center{};
    core::Vec3 half{};
    core::Vec3 broad_half{};
    core::Vec3 axis_x{1.0f, 0.0f, 0.0f};
    core::Vec3 axis_y{0.0f, 1.0f, 0.0f};
    core::Vec3 axis_z{0.0f, 0.0f, 1.0f};
    float radius{0.0f};
    ColliderShape shape{ColliderShape::Box};
    bool trigger{false};
    std::uint32_t layer{0};
    std::uint32_t collision_mask{0xffffffffu};
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

float dot(
    core::Vec3 a,
    core::Vec3 b) noexcept {

    return a.x * b.x +
        a.y * b.y +
        a.z * b.z;
}

core::Vec3 cross(
    core::Vec3 a,
    core::Vec3 b) noexcept {

    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

core::Vec3 scaled(
    core::Vec3 value,
    float scalar) noexcept {

    return {
        value.x * scalar,
        value.y * scalar,
        value.z * scalar
    };
}

core::Vec3 added(
    core::Vec3 a,
    core::Vec3 b) noexcept {

    return {
        a.x + b.x,
        a.y + b.y,
        a.z + b.z
    };
}

core::Vec3 normalized_axis(
    core::Vec3 value) noexcept {

    const float squared =
        dot(value, value);

    if (squared <=
        0.0000000001f) {
        return {};
    }

    return scaled(
        value,
        1.0f / std::sqrt(squared));
}

core::Quat normalized_quaternion(
    core::Quat value) noexcept {

    const float squared =
        value.x * value.x +
        value.y * value.y +
        value.z * value.z +
        value.w * value.w;

    if (squared <=
        0.0000000001f) {
        return {};
    }

    const float inverse =
        1.0f / std::sqrt(squared);

    return {
        value.x * inverse,
        value.y * inverse,
        value.z * inverse,
        value.w * inverse
    };
}

core::Vec3 rotate_vector(
    core::Quat rotation,
    core::Vec3 value) noexcept {

    rotation =
        normalized_quaternion(
            rotation);

    const core::Vec3 q{
        rotation.x,
        rotation.y,
        rotation.z
    };

    const auto first =
        cross(q, value);

    const auto second =
        cross(
            q,
            added(
                first,
                scaled(
                    value,
                    rotation.w)));

    return added(
        value,
        scaled(
            second,
            2.0f));
}

void box_axes(
    core::Quat rotation,
    bool is_2d,
    core::Vec3& axis_x,
    core::Vec3& axis_y,
    core::Vec3& axis_z) noexcept {

    if (is_2d) {
        rotation =
            normalized_quaternion(
                rotation);

        const float sine =
            2.0f *
            (rotation.w *
                 rotation.z +
             rotation.x *
                 rotation.y);

        const float cosine =
            1.0f -
            2.0f *
            (rotation.y *
                 rotation.y +
             rotation.z *
                 rotation.z);

        const float angle =
            std::atan2(
                sine,
                cosine);

        const float c =
            std::cos(angle);
        const float s =
            std::sin(angle);

        axis_x = {
            c,
            s,
            0.0f
        };

        axis_y = {
            -s,
            c,
            0.0f
        };

        axis_z = {
            0.0f,
            0.0f,
            1.0f
        };

        return;
    }

    axis_x =
        normalized_axis(
            rotate_vector(
                rotation,
                {1.0f, 0.0f, 0.0f}));

    axis_y =
        normalized_axis(
            rotate_vector(
                rotation,
                {0.0f, 1.0f, 0.0f}));

    axis_z =
        normalized_axis(
            rotate_vector(
                rotation,
                {0.0f, 0.0f, 1.0f}));

    if (axis_x == core::Vec3{} ||
        axis_y == core::Vec3{} ||
        axis_z == core::Vec3{}) {
        axis_x = {
            1.0f, 0.0f, 0.0f};
        axis_y = {
            0.0f, 1.0f, 0.0f};
        axis_z = {
            0.0f, 0.0f, 1.0f};
    }
}

core::Vec3 oriented_offset(
    core::Vec3 local,
    const ColliderBounds& bounds,
    bool is_2d) noexcept {

    auto result =
        added(
            scaled(
                bounds.axis_x,
                local.x),
            scaled(
                bounds.axis_y,
                local.y));

    if (!is_2d) {
        result =
            added(
                result,
                scaled(
                    bounds.axis_z,
                    local.z));
    } else {
        result.z = local.z;
    }

    return result;
}

void update_broad_half(
    ColliderBounds& bounds,
    bool is_2d) noexcept {

    if (bounds.shape ==
        ColliderShape::Radial) {
        bounds.broad_half = {
            bounds.radius,
            bounds.radius,
            is_2d
                ? 0.0f
                : bounds.radius
        };
        return;
    }

    bounds.broad_half = {
        std::abs(bounds.axis_x.x) *
                bounds.half.x +
            std::abs(bounds.axis_y.x) *
                bounds.half.y +
            (is_2d
                ? 0.0f
                : std::abs(bounds.axis_z.x) *
                    bounds.half.z),
        std::abs(bounds.axis_x.y) *
                bounds.half.x +
            std::abs(bounds.axis_y.y) *
                bounds.half.y +
            (is_2d
                ? 0.0f
                : std::abs(bounds.axis_z.y) *
                    bounds.half.z),
        is_2d
            ? 0.0f
            : std::abs(bounds.axis_x.z) *
                    bounds.half.x +
                std::abs(bounds.axis_y.z) *
                    bounds.half.y +
                std::abs(bounds.axis_z.z) *
                    bounds.half.z
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
    result.shape =
        ColliderShape::Box;
    result.trigger =
        collider.is_trigger;
    result.layer =
        collider.layer;
    result.collision_mask =
        collider.collision_mask;

    const auto* transform =
        world.transform(entity);

    if (!transform) {
        return result;
    }

    const auto scale =
        abs_scale(
            transform->local_scale);

    box_axes(
        transform->local_rotation,
        is_2d,
        result.axis_x,
        result.axis_y,
        result.axis_z);

    const core::Vec3 scaled_center{
        collider.center.x * scale.x,
        collider.center.y * scale.y,
        collider.center.z * scale.z
    };

    const auto offset =
        oriented_offset(
            scaled_center,
            result,
            is_2d);

    result.center = {
        transform->local_position.x +
            offset.x,
        transform->local_position.y +
            offset.y,
        transform->local_position.z +
            offset.z
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

    update_broad_half(
        result,
        is_2d);

    return result;
}

bool layer_enabled(
bool layer_enabled(
    std::uint32_t mask,
    std::uint32_t layer) noexcept {

    return layer < 32u &&
        (mask &
         (std::uint32_t{1u} << layer)) != 0u;
}

bool collision_layers_allow(
    const ColliderBounds& a,
    const ColliderBounds& b) noexcept {

    return layer_enabled(
               a.collision_mask,
               b.layer) &&
           layer_enabled(
               b.collision_mask,
               a.layer);
}

float projected_box_radius(
    const ColliderBounds& box,
    core::Vec3 axis,
    bool is_2d) noexcept {

    return
        std::abs(
            dot(
                axis,
                box.axis_x)) *
            box.half.x +
        std::abs(
            dot(
                axis,
                box.axis_y)) *
            box.half.y +
        (is_2d
            ? 0.0f
            : std::abs(
                  dot(
                      axis,
                      box.axis_z)) *
                  box.half.z);
}

bool overlap_box_pair(
    const ColliderBounds& a,
    const ColliderBounds& b,
    bool is_2d,
    BoxOverlap& overlap) noexcept {

    const core::Vec3 delta{
        b.center.x - a.center.x,
        b.center.y - a.center.y,
        is_2d
            ? 0.0f
            : b.center.z - a.center.z
    };

    float minimum_penetration =
        std::numeric_limits<float>::max();

    core::Vec3 minimum_axis{
        1.0f,
        0.0f,
        0.0f
    };

    const auto test_axis =
        [&](core::Vec3 raw_axis) {

            const auto axis =
                normalized_axis(
                    raw_axis);

            if (axis ==
                core::Vec3{}) {
                return true;
            }

            const float distance =
                std::abs(
                    dot(
                        delta,
                        axis));

            const float penetration =
                projected_box_radius(
                    a,
                    axis,
                    is_2d) +
                projected_box_radius(
                    b,
                    axis,
                    is_2d) -
                distance;

            if (penetration <=
                0.0f) {
                return false;
            }

            if (penetration <
                minimum_penetration) {
                minimum_penetration =
                    penetration;

                minimum_axis =
                    dot(
                        delta,
                        axis) >= 0.0f
                        ? axis
                        : scaled(
                            axis,
                            -1.0f);
            }

            return true;
        };

    if (!test_axis(a.axis_x) ||
        !test_axis(a.axis_y) ||
        !test_axis(b.axis_x) ||
        !test_axis(b.axis_y)) {
        return false;
    }

    if (!is_2d) {
        if (!test_axis(a.axis_z) ||
            !test_axis(b.axis_z)) {
            return false;
        }

        const core::Vec3 a_axes[]{
            a.axis_x,
            a.axis_y,
            a.axis_z
        };

        const core::Vec3 b_axes[]{
            b.axis_x,
            b.axis_y,
            b.axis_z
        };

        for (const auto a_axis :
             a_axes) {
            for (const auto b_axis :
                 b_axes) {
                if (!test_axis(
                        cross(
                            a_axis,
                            b_axis))) {
                    return false;
                }
            }
        }
    }

    overlap = {
        a.entity,
        b.entity,
        minimum_axis,
        minimum_penetration,
        a.trigger || b.trigger,
        is_2d
    };

    return true;
}

template <typename Collider>
ColliderBounds make_radial_bounds(
    const core::World& world,
    core::Entity entity,
    const Collider& collider,
    bool is_2d) {

    ColliderBounds result;
    result.entity = entity;
    result.shape =
        ColliderShape::Radial;
    result.trigger =
        collider.is_trigger;
    result.layer =
        collider.layer;
    result.collision_mask =
        collider.collision_mask;

    const auto* transform =
        world.transform(entity);

    if (!transform) {
        return result;
    }

    const auto scale =
        abs_scale(
            transform->local_scale);

    const float radial_scale =
        is_2d
            ? std::max(
                scale.x,
                scale.y)
            : std::max(
                scale.x,
                std::max(
                    scale.y,
                    scale.z));

    box_axes(
        transform->local_rotation,
        is_2d,
        result.axis_x,
        result.axis_y,
        result.axis_z);

    const core::Vec3 scaled_center{
        collider.center.x * scale.x,
        collider.center.y * scale.y,
        collider.center.z * scale.z
    };

    const auto offset =
        oriented_offset(
            scaled_center,
            result,
            is_2d);

    result.center = {
        transform->local_position.x +
            offset.x,
        transform->local_position.y +
            offset.y,
        transform->local_position.z +
            offset.z
    };

    result.radius =
        std::abs(collider.radius) *
        radial_scale;

    result.half = {
        result.radius,
        result.radius,
        is_2d
            ? 0.0f
            : result.radius
    };

    update_broad_half(
        result,
        is_2d);

    return result;
}

float length_squared(
    core::Vec3 value,
    bool is_2d) noexcept {

    return value.x * value.x +
        value.y * value.y +
        (is_2d
            ? 0.0f
            : value.z * value.z);
}

core::Vec3 normalized_or_axis(
    core::Vec3 value,
    bool is_2d) noexcept {

    if (is_2d) {
        value.z = 0.0f;
    }

    const float squared =
        length_squared(
            value,
            is_2d);

    if (squared <= 0.0000000001f) {
        return {
            1.0f,
            0.0f,
            0.0f
        };
    }

    const float inverse =
        1.0f /
        std::sqrt(
            squared);

    return {
        value.x * inverse,
        value.y * inverse,
        is_2d
            ? 0.0f
            : value.z * inverse
    };
}

bool overlap_radial_pair(
    const ColliderBounds& a,
    const ColliderBounds& b,
    bool is_2d,
    BoxOverlap& overlap) noexcept {

    const core::Vec3 delta{
        b.center.x - a.center.x,
        b.center.y - a.center.y,
        is_2d
            ? 0.0f
            : b.center.z - a.center.z
    };

    const float radii =
        a.radius +
        b.radius;

    const float squared =
        length_squared(
            delta,
            is_2d);

    if (squared >=
        radii * radii) {
        return false;
    }

    const float distance =
        std::sqrt(
            std::max(
                0.0f,
                squared));

    overlap = {
        a.entity,
        b.entity,
        normalized_or_axis(
            delta,
            is_2d),
        radii - distance,
        a.trigger || b.trigger,
        is_2d
    };

    return overlap.penetration > 0.0f;
}

bool overlap_box_radial(
    const ColliderBounds& box,
    const ColliderBounds& radial,
    bool is_2d,
    bool box_is_first,
    BoxOverlap& overlap) noexcept {

    const core::Vec3 delta{
        radial.center.x -
            box.center.x,
        radial.center.y -
            box.center.y,
        is_2d
            ? 0.0f
            : radial.center.z -
                box.center.z
    };

    const float local_x =
        dot(
            delta,
            box.axis_x);

    const float local_y =
        dot(
            delta,
            box.axis_y);

    const float local_z =
        is_2d
            ? 0.0f
            : dot(
                delta,
                box.axis_z);

    const core::Vec3 closest_local{
        std::clamp(
            local_x,
            -box.half.x,
            box.half.x),
        std::clamp(
            local_y,
            -box.half.y,
            box.half.y),
        is_2d
            ? 0.0f
            : std::clamp(
                local_z,
                -box.half.z,
                box.half.z)
    };

    auto closest_world =
        added(
            box.center,
            scaled(
                box.axis_x,
                closest_local.x));

    closest_world =
        added(
            closest_world,
            scaled(
                box.axis_y,
                closest_local.y));

    if (!is_2d) {
        closest_world =
            added(
                closest_world,
                scaled(
                    box.axis_z,
                    closest_local.z));
    }

    const core::Vec3 separation{
        radial.center.x -
            closest_world.x,
        radial.center.y -
            closest_world.y,
        is_2d
            ? 0.0f
            : radial.center.z -
                closest_world.z
    };

    const float squared =
        length_squared(
            separation,
            is_2d);

    core::Vec3 normal{};
    float penetration = 0.0f;

    if (squared >
        0.0000000001f) {

        const float distance =
            std::sqrt(squared);

        if (distance >=
            radial.radius) {
            return false;
        }

        normal = {
            separation.x / distance,
            separation.y / distance,
            is_2d
                ? 0.0f
                : separation.z / distance
        };

        penetration =
            radial.radius -
            distance;
    } else {
        float face_distance =
            box.half.x -
            std::abs(local_x);

        normal =
            scaled(
                box.axis_x,
                axis_sign(
                    local_x));

        const float y_distance =
            box.half.y -
            std::abs(local_y);

        if (y_distance <
            face_distance) {
            face_distance =
                y_distance;
            normal =
                scaled(
                    box.axis_y,
                    axis_sign(
                        local_y));
        }

        if (!is_2d) {
            const float z_distance =
                box.half.z -
                std::abs(local_z);

            if (z_distance <
                face_distance) {
                face_distance =
                    z_distance;
                normal =
                    scaled(
                        box.axis_z,
                        axis_sign(
                            local_z));
            }
        }

        penetration =
            radial.radius +
            std::max(
                0.0f,
                face_distance);
    }

    if (!box_is_first) {
        normal =
            scaled(
                normal,
                -1.0f);
    }

    overlap = {
        box_is_first
            ? box.entity
            : radial.entity,
        box_is_first
            ? radial.entity
            : box.entity,
        normal,
        penetration,
        box.trigger ||
            radial.trigger,
        is_2d
    };

    return penetration > 0.0f;
}

bool overlap_pair(
bool overlap_pair(
    const ColliderBounds& a,
    const ColliderBounds& b,
    bool is_2d,
    BoxOverlap& overlap) noexcept {

    if (a.shape ==
            ColliderShape::Box &&
        b.shape ==
            ColliderShape::Box) {
        return overlap_box_pair(
            a,
            b,
            is_2d,
            overlap);
    }

    if (a.shape ==
            ColliderShape::Radial &&
        b.shape ==
            ColliderShape::Radial) {
        return overlap_radial_pair(
            a,
            b,
            is_2d,
            overlap);
    }

    if (a.shape ==
        ColliderShape::Box) {
        return overlap_box_radial(
            a,
            b,
            is_2d,
            true,
            overlap);
    }

    return overlap_box_radial(
        b,
        a,
        is_2d,
        false,
        overlap);
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
std::vector<ColliderBounds>
collect_radial_bounds(
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
            !collider->enabled ||
            collider->radius <= 0.0f) {
            continue;
        }

        result.push_back(
            make_radial_bounds(
                world,
                entity,
                *collider,
                is_2d));
    }

    return result;
}

void append_overlaps(
    std::vector<ColliderBounds> bounds,
    bool is_2d,
    std::vector<BoxOverlap>& overlaps,
    std::size_t& tested_pairs) {

    std::sort(
        bounds.begin(),
        bounds.end(),
        [](const ColliderBounds& a,
           const ColliderBounds& b) {

            const float a_min =
                a.center.x - a.broad_half.x;

            const float b_min =
                b.center.x - b.broad_half.x;

            if (a_min != b_min) {
                return a_min < b_min;
            }

            return a.entity.value <
                b.entity.value;
        });

    for (std::size_t i = 0;
         i < bounds.size();
         ++i) {

        const float maximum_x =
            bounds[i].center.x +
            bounds[i].broad_half.x;

        for (std::size_t j = i + 1;
             j < bounds.size();
             ++j) {

            const float minimum_x =
                bounds[j].center.x -
                bounds[j].broad_half.x;

            if (minimum_x >= maximum_x) {
                break;
            }

            if (bounds[i].entity ==
                    bounds[j].entity ||
                !collision_layers_allow(
                    bounds[i],
                    bounds[j])) {
                continue;
            }

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

    auto bounds_3d =
        collect_bounds<BoxCollider>(
            world,
            box_collider_type(),
            false);

    auto spheres =
        collect_radial_bounds<SphereCollider>(
            world,
            sphere_collider_type(),
            false);

    bounds_3d.insert(
        bounds_3d.end(),
        spheres.begin(),
        spheres.end());

    append_overlaps(
        std::move(bounds_3d),
        false,
        result.overlaps,
        result.tested_pairs_3d);

    auto bounds_2d =
        collect_bounds<BoxCollider2D>(
            world,
            box_collider2d_type(),
            true);

    auto circles =
        collect_radial_bounds<CircleCollider2D>(
            world,
            circle_collider2d_type(),
            true);

    bounds_2d.insert(
        bounds_2d.end(),
        circles.begin(),
        circles.end());

    append_overlaps(
        std::move(bounds_2d),
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

std::vector<core::Entity> overlap_box(
    const core::World& world,
    core::Vec3 center,
    core::Vec3 size,
    bool include_triggers,
    std::uint32_t layer_mask) {

    std::vector<core::Entity> result;

    if (size.x <= 0.0f ||
        size.y <= 0.0f ||
        size.z <= 0.0f) {
        return result;
    }

    ColliderBounds query;
    query.center = center;
    query.half = {
        size.x * 0.5f,
        size.y * 0.5f,
        size.z * 0.5f
    };

    auto bounds =
        collect_bounds<BoxCollider>(
            world,
            box_collider_type(),
            false);

    auto spheres =
        collect_radial_bounds<SphereCollider>(
            world,
            sphere_collider_type(),
            false);

    bounds.insert(
        bounds.end(),
        spheres.begin(),
        spheres.end());

    for (const auto& candidate :
         bounds) {

        if ((!include_triggers &&
             candidate.trigger) ||
            !layer_enabled(
                layer_mask,
                candidate.layer)) {
            continue;
        }

        BoxOverlap overlap;

        if (overlap_pair(
                query,
                candidate,
                false,
                overlap)) {
            result.push_back(
                candidate.entity);
        }
    }

    return result;
}

std::vector<core::Entity> overlap_box_2d(
    const core::World& world,
    core::Vec2 center,
    core::Vec2 size,
    bool include_triggers,
    std::uint32_t layer_mask) {

    std::vector<core::Entity> result;

    if (size.x <= 0.0f ||
        size.y <= 0.0f) {
        return result;
    }

    ColliderBounds query;
    query.center = {
        center.x,
        center.y,
        0.0f
    };
    query.half = {
        size.x * 0.5f,
        size.y * 0.5f,
        0.0f
    };

    auto bounds =
        collect_bounds<BoxCollider2D>(
            world,
            box_collider2d_type(),
            true);

    auto circles =
        collect_radial_bounds<CircleCollider2D>(
            world,
            circle_collider2d_type(),
            true);

    bounds.insert(
        bounds.end(),
        circles.begin(),
        circles.end());

    for (const auto& candidate :
         bounds) {

        if ((!include_triggers &&
             candidate.trigger) ||
            !layer_enabled(
                layer_mask,
                candidate.layer)) {
            continue;
        }

        BoxOverlap overlap;

        if (overlap_pair(
                query,
                candidate,
                true,
                overlap)) {
            result.push_back(
                candidate.entity);
        }
    }

    return result;
}


namespace {

bool ray_bounds(
    core::Vec3 origin,
    core::Vec3 direction,
    const ColliderBounds& bounds,
    float max_distance,
    bool is_2d,
    float& distance,
    core::Vec3& normal) noexcept {

    const auto minimum =
        core::Vec3{
            bounds.center.x - bounds.half.x,
            bounds.center.y - bounds.half.y,
            bounds.center.z - bounds.half.z
        };

    const auto maximum =
        core::Vec3{
            bounds.center.x + bounds.half.x,
            bounds.center.y + bounds.half.y,
            bounds.center.z + bounds.half.z
        };

    float t_min = 0.0f;
    float t_max = max_distance;
    core::Vec3 hit_normal{};

    const auto test_axis =
        [&](float o,
            float d,
            float min_value,
            float max_value,
            core::Vec3 negative_normal,
            core::Vec3 positive_normal) {

            constexpr float epsilon =
                0.000001f;

            if (std::abs(d) <= epsilon) {
                return o >= min_value &&
                       o <= max_value;
            }

            float t1 =
                (min_value - o) / d;
            float t2 =
                (max_value - o) / d;

            core::Vec3 enter_normal =
                negative_normal;

            if (t1 > t2) {
                std::swap(t1, t2);
                enter_normal =
                    positive_normal;
            }

            if (t1 > t_min) {
                t_min = t1;
                hit_normal =
                    enter_normal;
            }

            t_max =
                std::min(
                    t_max,
                    t2);

            return t_min <= t_max;
        };

    if (!test_axis(
            origin.x,
            direction.x,
            minimum.x,
            maximum.x,
            {-1.0f, 0.0f, 0.0f},
            {1.0f, 0.0f, 0.0f}) ||
        !test_axis(
            origin.y,
            direction.y,
            minimum.y,
            maximum.y,
            {0.0f, -1.0f, 0.0f},
            {0.0f, 1.0f, 0.0f})) {
        return false;
    }

    if (!is_2d &&
        !test_axis(
            origin.z,
            direction.z,
            minimum.z,
            maximum.z,
            {0.0f, 0.0f, -1.0f},
            {0.0f, 0.0f, 1.0f})) {
        return false;
    }

    if (t_max < 0.0f ||
        t_min > max_distance) {
        return false;
    }

    distance =
        std::max(
            0.0f,
            t_min);

    normal =
        hit_normal;

    return true;
}

bool ray_radial(
    core::Vec3 origin,
    core::Vec3 direction,
    const ColliderBounds& bounds,
    float max_distance,
    bool is_2d,
    float& distance,
    core::Vec3& normal) noexcept {

    core::Vec3 from_center{
        origin.x -
            bounds.center.x,
        origin.y -
            bounds.center.y,
        is_2d
            ? 0.0f
            : origin.z -
                bounds.center.z
    };

    const float c =
        length_squared(
            from_center,
            is_2d) -
        bounds.radius *
            bounds.radius;

    const float b =
        from_center.x *
            direction.x +
        from_center.y *
            direction.y +
        (is_2d
            ? 0.0f
            : from_center.z *
                direction.z);

    if (c > 0.0f &&
        b > 0.0f) {
        return false;
    }

    const float discriminant =
        b * b -
        c;

    if (discriminant < 0.0f) {
        return false;
    }

    float hit_distance =
        -b -
        std::sqrt(
            discriminant);

    if (hit_distance < 0.0f) {
        hit_distance = 0.0f;
    }

    if (hit_distance >
        max_distance) {
        return false;
    }

    distance =
        hit_distance;

    const core::Vec3 point{
        origin.x +
            direction.x *
                hit_distance,
        origin.y +
            direction.y *
                hit_distance,
        is_2d
            ? 0.0f
            : origin.z +
                direction.z *
                    hit_distance
    };

    const core::Vec3 outward{
        point.x -
            bounds.center.x,
        point.y -
            bounds.center.y,
        is_2d
            ? 0.0f
            : point.z -
                bounds.center.z
    };

    if (length_squared(
            outward,
            is_2d) <=
        0.0000000001f) {
        normal = {
            -direction.x,
            -direction.y,
            is_2d
                ? 0.0f
                : -direction.z
        };
    } else {
        normal =
            normalized_or_axis(
                outward,
                is_2d);
    }

    return true;
}

core::Vec3 normalized_direction(
    core::Vec3 direction,
    bool is_2d) noexcept {

    if (is_2d) {
        direction.z = 0.0f;
    }

    const float length_squared =
        direction.x * direction.x +
        direction.y * direction.y +
        direction.z * direction.z;

    if (length_squared <=
        0.0000000001f) {
        return {};
    }

    const float inverse_length =
        1.0f /
        std::sqrt(
            length_squared);

    return {
        direction.x * inverse_length,
        direction.y * inverse_length,
        direction.z * inverse_length
    };
}

template <typename Collider>
std::optional<RaycastHit> raycast_bounds(
    const core::World& world,
    core::ComponentTypeId type,
    core::Vec3 origin,
    core::Vec3 direction,
    float max_distance,
    bool include_triggers,
    std::uint32_t layer_mask,
    bool is_2d) {

    if (!std::isfinite(max_distance) ||
        max_distance < 0.0f) {
        return std::nullopt;
    }

    const auto normalized =
        normalized_direction(
            direction,
            is_2d);

    if (normalized ==
        core::Vec3{}) {
        return std::nullopt;
    }

    const auto bounds =
        collect_bounds<Collider>(
            world,
            type,
            is_2d);

    std::optional<RaycastHit>
        closest;

    for (const auto& candidate :
         bounds) {

        if ((!include_triggers &&
             candidate.trigger) ||
            !layer_enabled(
                layer_mask,
                candidate.layer)) {
            continue;
        }

        float distance = 0.0f;
        core::Vec3 normal{};

        if (!ray_bounds(
                origin,
                normalized,
                candidate,
                max_distance,
                is_2d,
                distance,
                normal)) {
            continue;
        }

        if (closest &&
            distance >=
                closest->distance) {
            continue;
        }

        closest =
            RaycastHit{
                candidate.entity,
                {
                    origin.x +
                        normalized.x *
                        distance,
                    origin.y +
                        normalized.y *
                        distance,
                    origin.z +
                        normalized.z *
                        distance
                },
                normal,
                distance,
                candidate.trigger,
                is_2d,
                candidate.layer
            };
    }

    return closest;
}

template <typename Collider>
std::optional<RaycastHit>
raycast_radial_bounds(
    const core::World& world,
    core::ComponentTypeId type,
    core::Vec3 origin,
    core::Vec3 direction,
    float max_distance,
    bool include_triggers,
    std::uint32_t layer_mask,
    bool is_2d) {

    if (!std::isfinite(max_distance) ||
        max_distance < 0.0f) {
        return std::nullopt;
    }

    const auto normalized =
        normalized_direction(
            direction,
            is_2d);

    if (normalized ==
        core::Vec3{}) {
        return std::nullopt;
    }

    const auto bounds =
        collect_radial_bounds<Collider>(
            world,
            type,
            is_2d);

    std::optional<RaycastHit>
        closest;

    for (const auto& candidate :
         bounds) {

        if ((!include_triggers &&
             candidate.trigger) ||
            !layer_enabled(
                layer_mask,
                candidate.layer)) {
            continue;
        }

        float distance = 0.0f;
        core::Vec3 normal{};

        if (!ray_radial(
                origin,
                normalized,
                candidate,
                max_distance,
                is_2d,
                distance,
                normal)) {
            continue;
        }

        if (closest &&
            distance >=
                closest->distance) {
            continue;
        }

        closest =
            RaycastHit{
                candidate.entity,
                {
                    origin.x +
                        normalized.x *
                        distance,
                    origin.y +
                        normalized.y *
                        distance,
                    is_2d
                        ? 0.0f
                        : origin.z +
                            normalized.z *
                            distance
                },
                normal,
                distance,
                candidate.trigger,
                is_2d,
                candidate.layer
            };
    }

    return closest;
}

std::optional<RaycastHit> nearest_hit(
    std::optional<RaycastHit> first,
    std::optional<RaycastHit> second) {

    if (!first) return second;
    if (!second) return first;

    return second->distance <
            first->distance
        ? second
        : first;
}

} // namespace

std::optional<RaycastHit> raycast(
    const core::World& world,
    core::Vec3 origin,
    core::Vec3 direction,
    float max_distance,
    bool include_triggers,
    std::uint32_t layer_mask) {

    return nearest_hit(
        raycast_bounds<BoxCollider>(
            world,
            box_collider_type(),
            origin,
            direction,
            max_distance,
            include_triggers,
            layer_mask,
            false),
        raycast_radial_bounds<SphereCollider>(
            world,
            sphere_collider_type(),
            origin,
            direction,
            max_distance,
            include_triggers,
            layer_mask,
            false));
}

std::optional<RaycastHit> raycast_2d(
    const core::World& world,
    core::Vec2 origin,
    core::Vec2 direction,
    float max_distance,
    bool include_triggers,
    std::uint32_t layer_mask) {

    const core::Vec3 origin_3d{
        origin.x,
        origin.y,
        0.0f
    };

    const core::Vec3 direction_3d{
        direction.x,
        direction.y,
        0.0f
    };

    return nearest_hit(
        raycast_bounds<BoxCollider2D>(
            world,
            box_collider2d_type(),
            origin_3d,
            direction_3d,
            max_distance,
            include_triggers,
            layer_mask,
            true),
        raycast_radial_bounds<CircleCollider2D>(
            world,
            circle_collider2d_type(),
            origin_3d,
            direction_3d,
            max_distance,
            include_triggers,
            layer_mask,
            true));
}


namespace {

template <typename Collider>
std::optional<RaycastHit> box_cast_bounds(
    const core::World& world,
    core::ComponentTypeId type,
    core::Vec3 origin,
    core::Vec3 size,
    core::Vec3 direction,
    float max_distance,
    bool include_triggers,
    std::uint32_t layer_mask,
    bool is_2d) {

    if (size.x <= 0.0f ||
        size.y <= 0.0f ||
        (!is_2d && size.z <= 0.0f) ||
        !std::isfinite(max_distance) ||
        max_distance < 0.0f) {
        return std::nullopt;
    }

    const auto normalized =
        normalized_direction(
            direction,
            is_2d);

    if (normalized ==
        core::Vec3{}) {
        return std::nullopt;
    }

    const core::Vec3 cast_half{
        size.x * 0.5f,
        size.y * 0.5f,
        is_2d
            ? 0.0f
            : size.z * 0.5f
    };

    const auto bounds =
        collect_bounds<Collider>(
            world,
            type,
            is_2d);

    std::optional<RaycastHit>
        closest;

    for (const auto& candidate :
         bounds) {

        if ((!include_triggers &&
             candidate.trigger) ||
            !layer_enabled(
                layer_mask,
                candidate.layer)) {
            continue;
        }

        auto expanded =
            candidate;

        expanded.half.x +=
            cast_half.x;
        expanded.half.y +=
            cast_half.y;

        if (!is_2d) {
            expanded.half.z +=
                cast_half.z;
        }

        float distance = 0.0f;
        core::Vec3 normal{};

        if (!ray_bounds(
                origin,
                normalized,
                expanded,
                max_distance,
                is_2d,
                distance,
                normal)) {
            continue;
        }

        if (closest &&
            distance >=
                closest->distance) {
            continue;
        }

        // For this initial axis-aligned cast foundation, point is
        // the cast box center at first time of impact.
        closest =
            RaycastHit{
                candidate.entity,
                {
                    origin.x +
                        normalized.x *
                        distance,
                    origin.y +
                        normalized.y *
                        distance,
                    origin.z +
                        normalized.z *
                        distance
                },
                normal,
                distance,
                candidate.trigger,
                is_2d,
                candidate.layer
            };
    }

    return closest;
}

} // namespace

std::optional<RaycastHit> box_cast(
    const core::World& world,
    core::Vec3 origin,
    core::Vec3 size,
    core::Vec3 direction,
    float max_distance,
    bool include_triggers,
    std::uint32_t layer_mask) {

    return box_cast_bounds<BoxCollider>(
        world,
        box_collider_type(),
        origin,
        size,
        direction,
        max_distance,
        include_triggers,
        layer_mask,
        false);
}

std::optional<RaycastHit> box_cast_2d(
    const core::World& world,
    core::Vec2 origin,
    core::Vec2 size,
    core::Vec2 direction,
    float max_distance,
    bool include_triggers,
    std::uint32_t layer_mask) {

    return box_cast_bounds<BoxCollider2D>(
        world,
        box_collider2d_type(),
        {
            origin.x,
            origin.y,
            0.0f
        },
        {
            size.x,
            size.y,
            0.0f
        },
        {
            direction.x,
            direction.y,
            0.0f
        },
        max_distance,
        include_triggers,
        layer_mask,
        true);
}

} // namespace nengine::physics
