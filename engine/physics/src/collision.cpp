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
    Radial,
    Capsule
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
    core::Vec3 segment_a{};
    core::Vec3 segment_b{};
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

    if (bounds.shape ==
        ColliderShape::Capsule) {
        const core::Vec3 half_segment{
            std::max(
                std::abs(
                    bounds.segment_a.x -
                    bounds.center.x),
                std::abs(
                    bounds.segment_b.x -
                    bounds.center.x)),
            std::max(
                std::abs(
                    bounds.segment_a.y -
                    bounds.center.y),
                std::abs(
                    bounds.segment_b.y -
                    bounds.center.y)),
            is_2d
                ? 0.0f
                : std::max(
                    std::abs(
                        bounds.segment_a.z -
                        bounds.center.z),
                    std::abs(
                        bounds.segment_b.z -
                        bounds.center.z))
        };

        bounds.broad_half = {
            half_segment.x +
                bounds.radius,
            half_segment.y +
                bounds.radius,
            is_2d
                ? 0.0f
                : half_segment.z +
                    bounds.radius
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


ColliderBounds make_capsule_bounds(
    const core::World& world,
    core::Entity entity,
    const CapsuleCollider& collider) {

    ColliderBounds result;
    result.entity = entity;
    result.shape =
        ColliderShape::Capsule;
    result.trigger =
        collider.is_trigger;
    result.layer =
        collider.layer;
    result.collision_mask =
        collider.collision_mask;

    const auto* transform =
        world.transform(entity);

    if (!transform) return result;

    const auto scale =
        abs_scale(
            transform->local_scale);

    box_axes(
        transform->local_rotation,
        false,
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
            false);

    result.center = {
        transform->local_position.x +
            offset.x,
        transform->local_position.y +
            offset.y,
        transform->local_position.z +
            offset.z
    };

    core::Vec3 axis =
        result.axis_y;
    float axial_scale =
        scale.y;
    float radial_scale =
        std::max(
            scale.x,
            scale.z);

    if (collider.direction == 0u) {
        axis = result.axis_x;
        axial_scale = scale.x;
        radial_scale =
            std::max(
                scale.y,
                scale.z);
    } else if (collider.direction == 2u) {
        axis = result.axis_z;
        axial_scale = scale.z;
        radial_scale =
            std::max(
                scale.x,
                scale.y);
    }

    result.radius =
        std::abs(
            collider.radius) *
        radial_scale;

    const float scaled_height =
        std::max(
            std::abs(
                collider.height) *
                axial_scale,
            result.radius * 2.0f);

    const float half_segment =
        std::max(
            0.0f,
            scaled_height * 0.5f -
                result.radius);

    result.segment_a =
        added(
            result.center,
            scaled(
                axis,
                -half_segment));

    result.segment_b =
        added(
            result.center,
            scaled(
                axis,
                half_segment));

    result.half = {
        result.radius,
        result.radius,
        result.radius
    };

    update_broad_half(
        result,
        false);

    return result;
}

ColliderBounds make_capsule2d_bounds(
    const core::World& world,
    core::Entity entity,
    const CapsuleCollider2D& collider) {

    ColliderBounds result;
    result.entity = entity;
    result.shape =
        ColliderShape::Capsule;
    result.trigger =
        collider.is_trigger;
    result.layer =
        collider.layer;
    result.collision_mask =
        collider.collision_mask;

    const auto* transform =
        world.transform(entity);

    if (!transform) return result;

    const auto scale =
        abs_scale(
            transform->local_scale);

    box_axes(
        transform->local_rotation,
        true,
        result.axis_x,
        result.axis_y,
        result.axis_z);

    const core::Vec3 scaled_center{
        collider.center.x * scale.x,
        collider.center.y * scale.y,
        0.0f
    };

    const auto offset =
        oriented_offset(
            scaled_center,
            result,
            true);

    result.center = {
        transform->local_position.x +
            offset.x,
        transform->local_position.y +
            offset.y,
        0.0f
    };

    const bool horizontal =
        collider.direction == 1u;

    const core::Vec3 axis =
        horizontal
            ? result.axis_x
            : result.axis_y;

    const float axial_size =
        horizontal
            ? std::abs(collider.size.x) *
                scale.x
            : std::abs(collider.size.y) *
                scale.y;

    const float cross_size =
        horizontal
            ? std::abs(collider.size.y) *
                scale.y
            : std::abs(collider.size.x) *
                scale.x;

    result.radius =
        std::min(
            cross_size * 0.5f,
            axial_size * 0.5f);

    const float half_segment =
        std::max(
            0.0f,
            axial_size * 0.5f -
                result.radius);

    result.segment_a =
        added(
            result.center,
            scaled(
                axis,
                -half_segment));

    result.segment_b =
        added(
            result.center,
            scaled(
                axis,
                half_segment));

    result.segment_a.z = 0.0f;
    result.segment_b.z = 0.0f;

    result.half = {
        result.radius,
        result.radius,
        0.0f
    };

    update_broad_half(
        result,
        true);

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

core::Vec3 support_point(
    const ColliderBounds& bounds,
    core::Vec3 direction,
    bool is_2d) noexcept {

    direction =
        normalized_or_axis(
            direction,
            is_2d);

    if (bounds.shape ==
        ColliderShape::Radial) {

        return {
            bounds.center.x +
                direction.x *
                    bounds.radius,
            bounds.center.y +
                direction.y *
                    bounds.radius,
            is_2d
                ? 0.0f
                : bounds.center.z +
                    direction.z *
                        bounds.radius
        };
    }

    if (bounds.shape ==
        ColliderShape::Capsule) {

        const float first_projection =
            dot(
                bounds.segment_a,
                direction);

        const float second_projection =
            dot(
                bounds.segment_b,
                direction);

        const auto endpoint =
            second_projection >
                    first_projection
                ? bounds.segment_b
                : bounds.segment_a;

        return {
            endpoint.x +
                direction.x *
                    bounds.radius,
            endpoint.y +
                direction.y *
                    bounds.radius,
            is_2d
                ? 0.0f
                : endpoint.z +
                    direction.z *
                        bounds.radius
        };
    }

    const auto support_extent =
        [](float projection,
           float half_extent) noexcept {

            constexpr float epsilon =
                0.000001f;

            if (projection > epsilon)
                return half_extent;

            if (projection < -epsilon)
                return -half_extent;

            return 0.0f;
        };

    auto result =
        bounds.center;

    result =
        added(
            result,
            scaled(
                bounds.axis_x,
                support_extent(
                    dot(
                        direction,
                        bounds.axis_x),
                    bounds.half.x)));

    result =
        added(
            result,
            scaled(
                bounds.axis_y,
                support_extent(
                    dot(
                        direction,
                        bounds.axis_y),
                    bounds.half.y)));

    if (!is_2d) {
        result =
            added(
                result,
                scaled(
                    bounds.axis_z,
                    support_extent(
                        dot(
                            direction,
                            bounds.axis_z),
                        bounds.half.z)));
    } else {
        result.z = 0.0f;
    }

    return result;
}

void populate_contact_manifold(
    const ColliderBounds& first,
    const ColliderBounds& second,
    bool is_2d,
    BoxOverlap& overlap) noexcept {

    if (overlap.penetration <=
            0.0f ||
        overlap.is_trigger) {
        return;
    }

    const auto first_support =
        support_point(
            first,
            overlap.normal,
            is_2d);

    const auto second_support =
        support_point(
            second,
            scaled(
                overlap.normal,
                -1.0f),
            is_2d);

    auto& contact =
        overlap.manifold.points[0];

    contact.point = {
        (first_support.x +
         second_support.x) *
            0.5f,
        (first_support.y +
         second_support.y) *
            0.5f,
        is_2d
            ? 0.0f
            : (first_support.z +
               second_support.z) *
                  0.5f
    };

    contact.penetration =
        overlap.penetration;

    overlap.manifold.count = 1u;
}

core::Vec3 closest_point_on_segment(
    core::Vec3 a,
    core::Vec3 b,
    core::Vec3 point,
    bool is_2d) noexcept {

    core::Vec3 ab{
        b.x - a.x,
        b.y - a.y,
        is_2d
            ? 0.0f
            : b.z - a.z
    };

    core::Vec3 ap{
        point.x - a.x,
        point.y - a.y,
        is_2d
            ? 0.0f
            : point.z - a.z
    };

    const float denominator =
        length_squared(
            ab,
            is_2d);

    if (denominator <=
        0.0000000001f) {
        return a;
    }

    const float t =
        std::clamp(
            dot(ap, ab) /
                denominator,
            0.0f,
            1.0f);

    return {
        a.x + ab.x * t,
        a.y + ab.y * t,
        is_2d
            ? 0.0f
            : a.z + ab.z * t
    };
}

void closest_segment_pair(
    core::Vec3 p1,
    core::Vec3 q1,
    core::Vec3 p2,
    core::Vec3 q2,
    bool is_2d,
    core::Vec3& first,
    core::Vec3& second) noexcept {

    if (is_2d) {
        p1.z = 0.0f;
        q1.z = 0.0f;
        p2.z = 0.0f;
        q2.z = 0.0f;
    }

    const core::Vec3 d1{
        q1.x - p1.x,
        q1.y - p1.y,
        q1.z - p1.z
    };

    const core::Vec3 d2{
        q2.x - p2.x,
        q2.y - p2.y,
        q2.z - p2.z
    };

    const core::Vec3 r{
        p1.x - p2.x,
        p1.y - p2.y,
        p1.z - p2.z
    };

    const float a =
        dot(d1, d1);
    const float e =
        dot(d2, d2);
    const float f =
        dot(d2, r);

    constexpr float epsilon =
        0.0000000001f;

    float first_t = 0.0f;
    float second_t = 0.0f;

    if (a <= epsilon &&
        e <= epsilon) {
        first = p1;
        second = p2;
        return;
    }

    if (a <= epsilon) {
        second_t =
            std::clamp(
                f / e,
                0.0f,
                1.0f);
    } else {
        const float c =
            dot(d1, r);

        if (e <= epsilon) {
            first_t =
                std::clamp(
                    -c / a,
                    0.0f,
                    1.0f);
        } else {
            const float b =
                dot(d1, d2);

            const float denominator =
                a * e -
                b * b;

            if (std::abs(denominator) >
                epsilon) {
                first_t =
                    std::clamp(
                        (b * f -
                         c * e) /
                            denominator,
                        0.0f,
                        1.0f);
            }

            second_t =
                (b * first_t +
                 f) /
                e;

            if (second_t < 0.0f) {
                second_t = 0.0f;
                first_t =
                    std::clamp(
                        -c / a,
                        0.0f,
                        1.0f);
            } else if (
                second_t > 1.0f) {
                second_t = 1.0f;
                first_t =
                    std::clamp(
                        (b - c) / a,
                        0.0f,
                        1.0f);
            }
        }
    }

    first = {
        p1.x + d1.x * first_t,
        p1.y + d1.y * first_t,
        is_2d
            ? 0.0f
            : p1.z + d1.z * first_t
    };

    second = {
        p2.x + d2.x * second_t,
        p2.y + d2.y * second_t,
        is_2d
            ? 0.0f
            : p2.z + d2.z * second_t
    };
}

core::Vec3 box_local_point(
    const ColliderBounds& box,
    core::Vec3 point,
    bool is_2d) noexcept {

    const core::Vec3 delta{
        point.x - box.center.x,
        point.y - box.center.y,
        is_2d
            ? 0.0f
            : point.z - box.center.z
    };

    return {
        dot(delta, box.axis_x),
        dot(delta, box.axis_y),
        is_2d
            ? 0.0f
            : dot(delta, box.axis_z)
    };
}

core::Vec3 box_world_point(
    const ColliderBounds& box,
    core::Vec3 local,
    bool is_2d) noexcept {

    auto result =
        added(
            box.center,
            scaled(
                box.axis_x,
                local.x));

    result =
        added(
            result,
            scaled(
                box.axis_y,
                local.y));

    if (!is_2d) {
        result =
            added(
                result,
                scaled(
                    box.axis_z,
                    local.z));
    }

    return result;
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

bool overlap_capsule_radial(
    const ColliderBounds& capsule,
    const ColliderBounds& radial,
    bool is_2d,
    bool capsule_is_first,
    BoxOverlap& overlap) noexcept {

    const auto closest =
        closest_point_on_segment(
            capsule.segment_a,
            capsule.segment_b,
            radial.center,
            is_2d);

    const core::Vec3 separation{
        radial.center.x -
            closest.x,
        radial.center.y -
            closest.y,
        is_2d
            ? 0.0f
            : radial.center.z -
                closest.z
    };

    const float radii =
        capsule.radius +
        radial.radius;

    const float squared =
        length_squared(
            separation,
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

    auto normal =
        normalized_or_axis(
            squared >
                0.0000000001f
                ? separation
                : core::Vec3{
                    radial.center.x -
                        capsule.center.x,
                    radial.center.y -
                        capsule.center.y,
                    is_2d
                        ? 0.0f
                        : radial.center.z -
                            capsule.center.z
                },
            is_2d);

    if (!capsule_is_first) {
        normal =
            scaled(normal, -1.0f);
    }

    overlap = {
        capsule_is_first
            ? capsule.entity
            : radial.entity,
        capsule_is_first
            ? radial.entity
            : capsule.entity,
        normal,
        radii - distance,
        capsule.trigger ||
            radial.trigger,
        is_2d
    };

    return overlap.penetration >
        0.0f;
}

bool overlap_capsule_pair(
    const ColliderBounds& a,
    const ColliderBounds& b,
    bool is_2d,
    BoxOverlap& overlap) noexcept {

    core::Vec3 first{};
    core::Vec3 second{};

    closest_segment_pair(
        a.segment_a,
        a.segment_b,
        b.segment_a,
        b.segment_b,
        is_2d,
        first,
        second);

    const core::Vec3 separation{
        second.x - first.x,
        second.y - first.y,
        is_2d
            ? 0.0f
            : second.z - first.z
    };

    const float radii =
        a.radius +
        b.radius;

    const float squared =
        length_squared(
            separation,
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
            squared >
                0.0000000001f
                ? separation
                : core::Vec3{
                    b.center.x -
                        a.center.x,
                    b.center.y -
                        a.center.y,
                    is_2d
                        ? 0.0f
                        : b.center.z -
                            a.center.z
                },
            is_2d),
        radii - distance,
        a.trigger || b.trigger,
        is_2d
    };

    return overlap.penetration >
        0.0f;
}

bool overlap_box_capsule(
    const ColliderBounds& box,
    const ColliderBounds& capsule,
    bool is_2d,
    bool box_is_first,
    BoxOverlap& overlap) noexcept {

    const auto local_a =
        box_local_point(
            box,
            capsule.segment_a,
            is_2d);

    const auto local_b =
        box_local_point(
            box,
            capsule.segment_b,
            is_2d);

    const core::Vec3 local_delta{
        local_b.x - local_a.x,
        local_b.y - local_a.y,
        is_2d
            ? 0.0f
            : local_b.z - local_a.z
    };

    const auto evaluate =
        [&](float t,
            core::Vec3* segment_point,
            core::Vec3* box_point) {

            core::Vec3 point{
                local_a.x +
                    local_delta.x * t,
                local_a.y +
                    local_delta.y * t,
                is_2d
                    ? 0.0f
                    : local_a.z +
                        local_delta.z * t
            };

            core::Vec3 clamped{
                std::clamp(
                    point.x,
                    -box.half.x,
                    box.half.x),
                std::clamp(
                    point.y,
                    -box.half.y,
                    box.half.y),
                is_2d
                    ? 0.0f
                    : std::clamp(
                        point.z,
                        -box.half.z,
                        box.half.z)
            };

            if (segment_point)
                *segment_point = point;
            if (box_point)
                *box_point = clamped;

            const core::Vec3 difference{
                point.x - clamped.x,
                point.y - clamped.y,
                is_2d
                    ? 0.0f
                    : point.z - clamped.z
            };

            return length_squared(
                difference,
                is_2d);
        };

    float low = 0.0f;
    float high = 1.0f;

    for (int iteration = 0;
         iteration < 32;
         ++iteration) {

        const float first_t =
            (low * 2.0f +
             high) /
            3.0f;

        const float second_t =
            (low +
             high * 2.0f) /
            3.0f;

        if (evaluate(
                first_t,
                nullptr,
                nullptr) <
            evaluate(
                second_t,
                nullptr,
                nullptr)) {
            high = second_t;
        } else {
            low = first_t;
        }
    }

    const float t =
        (low + high) *
        0.5f;

    core::Vec3 segment_local{};
    core::Vec3 box_local{};

    const float squared =
        evaluate(
            t,
            &segment_local,
            &box_local);

    core::Vec3 normal{};
    float penetration = 0.0f;

    if (squared >
        0.0000000001f) {

        const float distance =
            std::sqrt(squared);

        if (distance >=
            capsule.radius) {
            return false;
        }

        const core::Vec3 local_normal{
            (segment_local.x -
             box_local.x) /
                distance,
            (segment_local.y -
             box_local.y) /
                distance,
            is_2d
                ? 0.0f
                : (segment_local.z -
                   box_local.z) /
                    distance
        };

        normal =
            added(
                scaled(
                    box.axis_x,
                    local_normal.x),
                scaled(
                    box.axis_y,
                    local_normal.y));

        if (!is_2d) {
            normal =
                added(
                    normal,
                    scaled(
                        box.axis_z,
                        local_normal.z));
        }

        normal =
            normalized_or_axis(
                normal,
                is_2d);

        penetration =
            capsule.radius -
            distance;
    } else {
        float face_distance =
            box.half.x -
            std::abs(
                segment_local.x);

        core::Vec3 local_normal{
            axis_sign(
                segment_local.x),
            0.0f,
            0.0f
        };

        const float y_distance =
            box.half.y -
            std::abs(
                segment_local.y);

        if (y_distance <
            face_distance) {
            face_distance =
                y_distance;
            local_normal = {
                0.0f,
                axis_sign(
                    segment_local.y),
                0.0f
            };
        }

        if (!is_2d) {
            const float z_distance =
                box.half.z -
                std::abs(
                    segment_local.z);

            if (z_distance <
                face_distance) {
                face_distance =
                    z_distance;
                local_normal = {
                    0.0f,
                    0.0f,
                    axis_sign(
                        segment_local.z)
                };
            }
        }

        normal =
            added(
                scaled(
                    box.axis_x,
                    local_normal.x),
                scaled(
                    box.axis_y,
                    local_normal.y));

        if (!is_2d) {
            normal =
                added(
                    normal,
                    scaled(
                        box.axis_z,
                        local_normal.z));
        }

        penetration =
            capsule.radius +
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
            : capsule.entity,
        box_is_first
            ? capsule.entity
            : box.entity,
        normal,
        penetration,
        box.trigger ||
            capsule.trigger,
        is_2d
    };

    return penetration > 0.0f;
}

bool overlap_pair_raw(
    const ColliderBounds& a,
    const ColliderBounds& b,
    bool is_2d,
    BoxOverlap& overlap) noexcept {

    if (a.shape == ColliderShape::Box &&
        b.shape == ColliderShape::Box) {
        return overlap_box_pair(
            a,
            b,
            is_2d,
            overlap);
    }

    if (a.shape == ColliderShape::Radial &&
        b.shape == ColliderShape::Radial) {
        return overlap_radial_pair(
            a,
            b,
            is_2d,
            overlap);
    }

    if (a.shape == ColliderShape::Capsule &&
        b.shape == ColliderShape::Capsule) {
        return overlap_capsule_pair(
            a,
            b,
            is_2d,
            overlap);
    }

    if (a.shape == ColliderShape::Capsule &&
        b.shape == ColliderShape::Radial) {
        return overlap_capsule_radial(
            a,
            b,
            is_2d,
            true,
            overlap);
    }

    if (a.shape == ColliderShape::Radial &&
        b.shape == ColliderShape::Capsule) {
        return overlap_capsule_radial(
            b,
            a,
            is_2d,
            false,
            overlap);
    }

    if (a.shape == ColliderShape::Box &&
        b.shape == ColliderShape::Capsule) {
        return overlap_box_capsule(
            a,
            b,
            is_2d,
            true,
            overlap);
    }

    if (a.shape == ColliderShape::Capsule &&
        b.shape == ColliderShape::Box) {
        return overlap_box_capsule(
            b,
            a,
            is_2d,
            false,
            overlap);
    }

    if (a.shape == ColliderShape::Box) {
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

bool overlap_pair(
    const ColliderBounds& a,
    const ColliderBounds& b,
    bool is_2d,
    BoxOverlap& overlap) noexcept {

    BoxOverlap candidate;

    if (!overlap_pair_raw(
            a,
            b,
            is_2d,
            candidate)) {
        return false;
    }

    populate_contact_manifold(
        a,
        b,
        is_2d,
        candidate);

    overlap =
        candidate;

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

std::vector<ColliderBounds>
collect_capsule_bounds(
    const core::World& world) {

    std::vector<ColliderBounds> result;

    for (const auto entity :
         world.entities()) {

        if (!world.active(entity)) {
            continue;
        }

        const auto* collider =
            world.get_component<
                CapsuleCollider>(
                    entity,
                    capsule_collider_type());

        if (!collider ||
            !collider->enabled ||
            collider->radius <= 0.0f ||
            collider->height <
                collider->radius * 2.0f ||
            collider->direction > 2u) {
            continue;
        }

        result.push_back(
            make_capsule_bounds(
                world,
                entity,
                *collider));
    }

    return result;
}

std::vector<ColliderBounds>
collect_capsule2d_bounds(
    const core::World& world) {

    std::vector<ColliderBounds> result;

    for (const auto entity :
         world.entities()) {

        if (!world.active(entity)) {
            continue;
        }

        const auto* collider =
            world.get_component<
                CapsuleCollider2D>(
                    entity,
                    capsule_collider2d_type());

        if (!collider ||
            !collider->enabled ||
            collider->size.x <= 0.0f ||
            collider->size.y <= 0.0f ||
            collider->direction > 1u) {
            continue;
        }

        result.push_back(
            make_capsule2d_bounds(
                world,
                entity,
                *collider));
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

    auto capsules =
        collect_capsule_bounds(
            world);

    bounds_3d.insert(
        bounds_3d.end(),
        capsules.begin(),
        capsules.end());

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

    auto capsules_2d =
        collect_capsule2d_bounds(
            world);

    bounds_2d.insert(
        bounds_2d.end(),
        capsules_2d.begin(),
        capsules_2d.end());

    append_overlaps(
        std::move(bounds_2d),
        true,
        result.overlaps,
        result.tested_pairs_2d);

    return result;
}

namespace {

struct ContactMaterial {
    float friction{0.5f};
    float restitution{0.0f};
};

ContactMaterial contact_material_3d(
    const core::World& world,
    core::Entity entity) noexcept {

    if (const auto* collider =
            world.get_component<BoxCollider>(
                entity,
                box_collider_type())) {
        return {
            collider->friction,
            collider->restitution
        };
    }

    if (const auto* collider =
            world.get_component<SphereCollider>(
                entity,
                sphere_collider_type())) {
        return {
            collider->friction,
            collider->restitution
        };
    }

    if (const auto* collider =
            world.get_component<CapsuleCollider>(
                entity,
                capsule_collider_type())) {
        return {
            collider->friction,
            collider->restitution
        };
    }

    return {};
}

ContactMaterial contact_material_2d(
    const core::World& world,
    core::Entity entity) noexcept {

    if (const auto* collider =
            world.get_component<BoxCollider2D>(
                entity,
                box_collider2d_type())) {
        return {
            collider->friction,
            collider->restitution
        };
    }

    if (const auto* collider =
            world.get_component<CircleCollider2D>(
                entity,
                circle_collider2d_type())) {
        return {
            collider->friction,
            collider->restitution
        };
    }

    if (const auto* collider =
            world.get_component<CapsuleCollider2D>(
                entity,
                capsule_collider2d_type())) {
        return {
            collider->friction,
            collider->restitution
        };
    }

    return {};
}

} // namespace

CollisionResolutionStats resolve_box_contacts_3d(
    core::World& world,
    const std::vector<BoxOverlap>& overlaps) {

    CollisionResolutionStats stats;

    const auto dot3 =
        [](core::Vec3 a,
           core::Vec3 b) noexcept {
            return a.x * b.x +
                a.y * b.y +
                a.z * b.z;
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

        const float first_inverse_mass =
            first_dynamic &&
            first_body->mass > 0.0f
                ? 1.0f /
                    first_body->mass
                : 0.0f;

        const float second_inverse_mass =
            second_dynamic &&
            second_body->mass > 0.0f
                ? 1.0f /
                    second_body->mass
                : 0.0f;

        const float inverse_mass_sum =
            first_inverse_mass +
            second_inverse_mass;

        if (inverse_mass_sum <= 0.0f) {
            continue;
        }

        const float first_share =
            first_inverse_mass /
            inverse_mass_sum;

        const float second_share =
            second_inverse_mass /
            inverse_mass_sum;

        constexpr float contact_slop =
            0.00001f;

        const float correction =
            std::max(
                0.0f,
                overlap.penetration -
                    contact_slop);

        first_transform->local_position.x -=
            overlap.normal.x *
            correction *
            first_share;
        first_transform->local_position.y -=
            overlap.normal.y *
            correction *
            first_share;
        first_transform->local_position.z -=
            overlap.normal.z *
            correction *
            first_share;

        second_transform->local_position.x +=
            overlap.normal.x *
            correction *
            second_share;
        second_transform->local_position.y +=
            overlap.normal.y *
            correction *
            second_share;
        second_transform->local_position.z +=
            overlap.normal.z *
            correction *
            second_share;

        const core::Vec3 first_velocity =
            first_dynamic
                ? first_body->linear_velocity
                : core::Vec3{};

        const core::Vec3 second_velocity =
            second_dynamic
                ? second_body->linear_velocity
                : core::Vec3{};

        core::Vec3 relative{
            second_velocity.x -
                first_velocity.x,
            second_velocity.y -
                first_velocity.y,
            second_velocity.z -
                first_velocity.z
        };

        const float normal_speed =
            dot3(
                relative,
                overlap.normal);

        if (normal_speed < 0.0f) {
            const auto first_material =
                contact_material_3d(
                    world,
                    overlap.first);

            const auto second_material =
                contact_material_3d(
                    world,
                    overlap.second);

            const float restitution =
                std::max(
                    first_material.restitution,
                    second_material.restitution);

            const float normal_impulse =
                -(1.0f + restitution) *
                normal_speed /
                inverse_mass_sum;

            if (first_dynamic) {
                first_body->linear_velocity.x -=
                    overlap.normal.x *
                    normal_impulse *
                    first_inverse_mass;
                first_body->linear_velocity.y -=
                    overlap.normal.y *
                    normal_impulse *
                    first_inverse_mass;
                first_body->linear_velocity.z -=
                    overlap.normal.z *
                    normal_impulse *
                    first_inverse_mass;
            }

            if (second_dynamic) {
                second_body->linear_velocity.x +=
                    overlap.normal.x *
                    normal_impulse *
                    second_inverse_mass;
                second_body->linear_velocity.y +=
                    overlap.normal.y *
                    normal_impulse *
                    second_inverse_mass;
                second_body->linear_velocity.z +=
                    overlap.normal.z *
                    normal_impulse *
                    second_inverse_mass;
            }

            const core::Vec3 post_first =
                first_dynamic
                    ? first_body->linear_velocity
                    : core::Vec3{};

            const core::Vec3 post_second =
                second_dynamic
                    ? second_body->linear_velocity
                    : core::Vec3{};

            relative = {
                post_second.x -
                    post_first.x,
                post_second.y -
                    post_first.y,
                post_second.z -
                    post_first.z
            };

            const float post_normal_speed =
                dot3(
                    relative,
                    overlap.normal);

            core::Vec3 tangent{
                relative.x -
                    overlap.normal.x *
                    post_normal_speed,
                relative.y -
                    overlap.normal.y *
                    post_normal_speed,
                relative.z -
                    overlap.normal.z *
                    post_normal_speed
            };

            const float tangent_squared =
                dot3(
                    tangent,
                    tangent);

            if (tangent_squared >
                0.0000000001f) {

                const float inverse_tangent =
                    1.0f /
                    std::sqrt(
                        tangent_squared);

                tangent.x *=
                    inverse_tangent;
                tangent.y *=
                    inverse_tangent;
                tangent.z *=
                    inverse_tangent;

                float tangent_impulse =
                    -dot3(
                        relative,
                        tangent) /
                    inverse_mass_sum;

                const float friction =
                    std::sqrt(
                        std::max(
                            0.0f,
                            first_material.friction *
                            second_material.friction));

                const float limit =
                    normal_impulse *
                    friction;

                tangent_impulse =
                    std::clamp(
                        tangent_impulse,
                        -limit,
                        limit);

                if (first_dynamic) {
                    first_body->linear_velocity.x -=
                        tangent.x *
                        tangent_impulse *
                        first_inverse_mass;
                    first_body->linear_velocity.y -=
                        tangent.y *
                        tangent_impulse *
                        first_inverse_mass;
                    first_body->linear_velocity.z -=
                        tangent.z *
                        tangent_impulse *
                        first_inverse_mass;
                }

                if (second_dynamic) {
                    second_body->linear_velocity.x +=
                        tangent.x *
                        tangent_impulse *
                        second_inverse_mass;
                    second_body->linear_velocity.y +=
                        tangent.y *
                        tangent_impulse *
                        second_inverse_mass;
                    second_body->linear_velocity.z +=
                        tangent.z *
                        tangent_impulse *
                        second_inverse_mass;
                }
            }
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

        const float first_inverse_mass =
            first_dynamic &&
            first_body->mass > 0.0f
                ? 1.0f /
                    first_body->mass
                : 0.0f;

        const float second_inverse_mass =
            second_dynamic &&
            second_body->mass > 0.0f
                ? 1.0f /
                    second_body->mass
                : 0.0f;

        const float inverse_mass_sum =
            first_inverse_mass +
            second_inverse_mass;

        if (inverse_mass_sum <= 0.0f) {
            continue;
        }

        const float first_share =
            first_inverse_mass /
            inverse_mass_sum;

        const float second_share =
            second_inverse_mass /
            inverse_mass_sum;

        constexpr float contact_slop =
            0.00001f;

        const float correction =
            std::max(
                0.0f,
                overlap.penetration -
                    contact_slop);

        first_transform->local_position.x -=
            overlap.normal.x *
            correction *
            first_share;
        first_transform->local_position.y -=
            overlap.normal.y *
            correction *
            first_share;

        second_transform->local_position.x +=
            overlap.normal.x *
            correction *
            second_share;
        second_transform->local_position.y +=
            overlap.normal.y *
            correction *
            second_share;

        const core::Vec3 first_velocity =
            first_dynamic
                ? first_body->linear_velocity
                : core::Vec3{};

        const core::Vec3 second_velocity =
            second_dynamic
                ? second_body->linear_velocity
                : core::Vec3{};

        core::Vec3 relative{
            second_velocity.x -
                first_velocity.x,
            second_velocity.y -
                first_velocity.y,
            0.0f
        };

        const float normal_speed =
            dot2(
                relative,
                overlap.normal);

        if (normal_speed < 0.0f) {
            const auto first_material =
                contact_material_2d(
                    world,
                    overlap.first);

            const auto second_material =
                contact_material_2d(
                    world,
                    overlap.second);

            const float restitution =
                std::max(
                    first_material.restitution,
                    second_material.restitution);

            const float normal_impulse =
                -(1.0f + restitution) *
                normal_speed /
                inverse_mass_sum;

            if (first_dynamic) {
                first_body->linear_velocity.x -=
                    overlap.normal.x *
                    normal_impulse *
                    first_inverse_mass;
                first_body->linear_velocity.y -=
                    overlap.normal.y *
                    normal_impulse *
                    first_inverse_mass;
            }

            if (second_dynamic) {
                second_body->linear_velocity.x +=
                    overlap.normal.x *
                    normal_impulse *
                    second_inverse_mass;
                second_body->linear_velocity.y +=
                    overlap.normal.y *
                    normal_impulse *
                    second_inverse_mass;
            }

            const core::Vec3 post_first =
                first_dynamic
                    ? first_body->linear_velocity
                    : core::Vec3{};

            const core::Vec3 post_second =
                second_dynamic
                    ? second_body->linear_velocity
                    : core::Vec3{};

            relative = {
                post_second.x -
                    post_first.x,
                post_second.y -
                    post_first.y,
                0.0f
            };

            const float post_normal_speed =
                dot2(
                    relative,
                    overlap.normal);

            core::Vec3 tangent{
                relative.x -
                    overlap.normal.x *
                    post_normal_speed,
                relative.y -
                    overlap.normal.y *
                    post_normal_speed,
                0.0f
            };

            const float tangent_squared =
                dot2(
                    tangent,
                    tangent);

            if (tangent_squared >
                0.0000000001f) {

                const float inverse_tangent =
                    1.0f /
                    std::sqrt(
                        tangent_squared);

                tangent.x *=
                    inverse_tangent;
                tangent.y *=
                    inverse_tangent;

                float tangent_impulse =
                    -dot2(
                        relative,
                        tangent) /
                    inverse_mass_sum;

                const float friction =
                    std::sqrt(
                        std::max(
                            0.0f,
                            first_material.friction *
                            second_material.friction));

                const float limit =
                    normal_impulse *
                    friction;

                tangent_impulse =
                    std::clamp(
                        tangent_impulse,
                        -limit,
                        limit);

                if (first_dynamic) {
                    first_body->linear_velocity.x -=
                        tangent.x *
                        tangent_impulse *
                        first_inverse_mass;
                    first_body->linear_velocity.y -=
                        tangent.y *
                        tangent_impulse *
                        first_inverse_mass;
                }

                if (second_dynamic) {
                    second_body->linear_velocity.x +=
                        tangent.x *
                        tangent_impulse *
                        second_inverse_mass;
                    second_body->linear_velocity.y +=
                        tangent.y *
                        tangent_impulse *
                        second_inverse_mass;
                }
            }
        }

        if (first_dynamic) {
            first_body->linear_velocity.z =
                0.0f;
        }

        if (second_dynamic) {
            second_body->linear_velocity.z =
                0.0f;
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

    auto capsules =
        collect_capsule_bounds(
            world);

    bounds.insert(
        bounds.end(),
        capsules.begin(),
        capsules.end());

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

    auto capsules =
        collect_capsule2d_bounds(
            world);

    bounds.insert(
        bounds.end(),
        capsules.begin(),
        capsules.end());

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

    const core::Vec3 relative{
        origin.x -
            bounds.center.x,
        origin.y -
            bounds.center.y,
        is_2d
            ? 0.0f
            : origin.z -
                bounds.center.z
    };

    float t_min = 0.0f;
    float t_max =
        max_distance;
    core::Vec3 hit_normal{};

    const auto test_axis =
        [&](core::Vec3 axis,
            float half_extent) {

            const float o =
                dot(
                    relative,
                    axis);

            const float d =
                dot(
                    direction,
                    axis);

            constexpr float epsilon =
                0.000001f;

            if (std::abs(d) <=
                epsilon) {
                return std::abs(o) <=
                    half_extent;
            }

            float t1 =
                (-half_extent - o) /
                d;

            float t2 =
                (half_extent - o) /
                d;

            auto enter_normal =
                scaled(
                    axis,
                    -1.0f);

            if (t1 > t2) {
                std::swap(
                    t1,
                    t2);

                enter_normal =
                    axis;
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

            return t_min <=
                t_max;
        };

    if (!test_axis(
            bounds.axis_x,
            bounds.half.x) ||
        !test_axis(
            bounds.axis_y,
            bounds.half.y)) {
        return false;
    }

    if (!is_2d &&
        !test_axis(
            bounds.axis_z,
            bounds.half.z)) {
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

bool ray_capsule(
    core::Vec3 origin,
    core::Vec3 direction,
    const ColliderBounds& bounds,
    float max_distance,
    bool is_2d,
    float& distance,
    core::Vec3& normal) noexcept {

    const auto closest_origin =
        closest_point_on_segment(
            bounds.segment_a,
            bounds.segment_b,
            origin,
            is_2d);

    const core::Vec3 origin_offset{
        origin.x -
            closest_origin.x,
        origin.y -
            closest_origin.y,
        is_2d
            ? 0.0f
            : origin.z -
                closest_origin.z
    };

    if (length_squared(
            origin_offset,
            is_2d) <=
        bounds.radius *
            bounds.radius) {

        distance = 0.0f;
        normal = {
            -direction.x,
            -direction.y,
            is_2d
                ? 0.0f
                : -direction.z
        };
        return true;
    }

    core::Vec3 ba{
        bounds.segment_b.x -
            bounds.segment_a.x,
        bounds.segment_b.y -
            bounds.segment_a.y,
        is_2d
            ? 0.0f
            : bounds.segment_b.z -
                bounds.segment_a.z
    };

    core::Vec3 oa{
        origin.x -
            bounds.segment_a.x,
        origin.y -
            bounds.segment_a.y,
        is_2d
            ? 0.0f
            : origin.z -
                bounds.segment_a.z
    };

    const float baba =
        dot(ba, ba);

    if (baba <=
        0.0000000001f) {

        ColliderBounds radial =
            bounds;
        radial.center =
            bounds.segment_a;
        radial.shape =
            ColliderShape::Radial;

        return ray_radial(
            origin,
            direction,
            radial,
            max_distance,
            is_2d,
            distance,
            normal);
    }

    const float bard =
        dot(ba, direction);
    const float baoa =
        dot(ba, oa);
    const float rdoa =
        dot(direction, oa);
    const float oaoa =
        dot(oa, oa);

    const float quadratic_a =
        baba -
        bard * bard;
    const float quadratic_b =
        baba * rdoa -
        baoa * bard;
    const float quadratic_c =
        baba * oaoa -
        baoa * baoa -
        bounds.radius *
            bounds.radius *
            baba;

    float best_distance =
        std::numeric_limits<float>::max();
    core::Vec3 best_normal{};
    bool hit = false;

    const float discriminant =
        quadratic_b *
            quadratic_b -
        quadratic_a *
            quadratic_c;

    if (quadratic_a >
            0.0000000001f &&
        discriminant >= 0.0f) {

        const float t =
            (-quadratic_b -
             std::sqrt(
                 discriminant)) /
            quadratic_a;

        const float y =
            baoa +
            t * bard;

        if (t >= 0.0f &&
            t <= max_distance &&
            y > 0.0f &&
            y < baba) {

            const core::Vec3 point{
                origin.x +
                    direction.x * t,
                origin.y +
                    direction.y * t,
                is_2d
                    ? 0.0f
                    : origin.z +
                        direction.z * t
            };

            const float segment_t =
                y / baba;

            const core::Vec3 axis_point{
                bounds.segment_a.x +
                    ba.x * segment_t,
                bounds.segment_a.y +
                    ba.y * segment_t,
                is_2d
                    ? 0.0f
                    : bounds.segment_a.z +
                        ba.z * segment_t
            };

            best_distance = t;
            best_normal =
                normalized_or_axis(
                    {
                        point.x -
                            axis_point.x,
                        point.y -
                            axis_point.y,
                        is_2d
                            ? 0.0f
                            : point.z -
                                axis_point.z
                    },
                    is_2d);
            hit = true;
        }
    }

    const core::Vec3 endpoints[]{
        bounds.segment_a,
        bounds.segment_b
    };

    for (const auto endpoint :
         endpoints) {

        ColliderBounds radial =
            bounds;
        radial.center = endpoint;
        radial.shape =
            ColliderShape::Radial;

        float endpoint_distance =
            0.0f;
        core::Vec3 endpoint_normal{};

        if (!ray_radial(
                origin,
                direction,
                radial,
                max_distance,
                is_2d,
                endpoint_distance,
                endpoint_normal)) {
            continue;
        }

        if (!hit ||
            endpoint_distance <
                best_distance) {

            best_distance =
                endpoint_distance;
            best_normal =
                endpoint_normal;
            hit = true;
        }
    }

    if (!hit) return false;

    distance =
        best_distance;
    normal =
        best_normal;
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

std::optional<RaycastHit>
raycast_capsule_collection(
    const std::vector<ColliderBounds>& bounds,
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

        if (!ray_capsule(
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
        nearest_hit(
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
                false)),
        raycast_capsule_collection(
            collect_capsule_bounds(
                world),
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
        nearest_hit(
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
                true)),
        raycast_capsule_collection(
            collect_capsule2d_bounds(
                world),
            origin_3d,
            direction_3d,
            max_distance,
            include_triggers,
            layer_mask,
            true));
}


namespace {

float axis_aligned_box_radius(
    core::Vec3 half,
    core::Vec3 axis,
    bool is_2d) noexcept {

    return
        std::abs(axis.x) *
            half.x +
        std::abs(axis.y) *
            half.y +
        (is_2d
            ? 0.0f
            : std::abs(axis.z) *
                half.z);
}

bool swept_box_hit(
    core::Vec3 origin,
    core::Vec3 cast_half,
    core::Vec3 direction,
    const ColliderBounds& target,
    float max_distance,
    bool is_2d,
    float& distance,
    core::Vec3& normal) noexcept {

    const core::Vec3 separation{
        target.center.x -
            origin.x,
        target.center.y -
            origin.y,
        is_2d
            ? 0.0f
            : target.center.z -
                origin.z
    };

    float enter_time = 0.0f;
    float exit_time =
        max_distance;
    core::Vec3 enter_normal{};

    const auto test_axis =
        [&](core::Vec3 raw_axis) {

            const auto axis =
                normalized_axis(
                    raw_axis);

            if (axis ==
                core::Vec3{}) {
                return true;
            }

            const float center_distance =
                dot(
                    separation,
                    axis);

            const float velocity =
                dot(
                    direction,
                    axis);

            const float radius =
                axis_aligned_box_radius(
                    cast_half,
                    axis,
                    is_2d) +
                projected_box_radius(
                    target,
                    axis,
                    is_2d);

            constexpr float epsilon =
                0.000001f;

            if (std::abs(velocity) <=
                epsilon) {
                return std::abs(
                           center_distance) <=
                    radius;
            }

            float t1 =
                (center_distance -
                 radius) /
                velocity;

            float t2 =
                (center_distance +
                 radius) /
                velocity;

            if (t1 > t2) {
                std::swap(
                    t1,
                    t2);
            }

            if (t1 > enter_time) {
                enter_time =
                    t1;

                enter_normal =
                    velocity > 0.0f
                        ? scaled(
                            axis,
                            -1.0f)
                        : axis;
            }

            exit_time =
                std::min(
                    exit_time,
                    t2);

            return enter_time <=
                exit_time;
        };

    const core::Vec3 world_axes[]{
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f}
    };

    if (!test_axis(
            world_axes[0]) ||
        !test_axis(
            world_axes[1]) ||
        !test_axis(
            target.axis_x) ||
        !test_axis(
            target.axis_y)) {
        return false;
    }

    if (!is_2d) {
        if (!test_axis(
                world_axes[2]) ||
            !test_axis(
                target.axis_z)) {
            return false;
        }

        const core::Vec3 target_axes[]{
            target.axis_x,
            target.axis_y,
            target.axis_z
        };

        for (const auto world_axis :
             world_axes) {
            for (const auto target_axis :
                 target_axes) {
                if (!test_axis(
                        cross(
                            world_axis,
                            target_axis))) {
                    return false;
                }
            }
        }
    }

    if (exit_time < 0.0f ||
        enter_time >
            max_distance) {
        return false;
    }

    distance =
        std::max(
            0.0f,
            enter_time);

    normal =
        enter_time > 0.0f
            ? enter_normal
            : core::Vec3{};

    return true;
}

bool swept_aabb_interval(
    core::Vec3 origin,
    core::Vec3 cast_half,
    core::Vec3 direction,
    const ColliderBounds& target,
    float max_distance,
    bool is_2d,
    float& enter,
    float& exit) noexcept {

    enter = 0.0f;
    exit = max_distance;

    const core::Vec3 separation{
        target.center.x - origin.x,
        target.center.y - origin.y,
        is_2d
            ? 0.0f
            : target.center.z - origin.z
    };

    const auto test_axis =
        [&](float center_distance,
            float velocity,
            float radius) {

            constexpr float epsilon =
                0.000001f;

            if (std::abs(velocity) <=
                epsilon) {
                return std::abs(center_distance) <=
                    radius;
            }

            float first =
                (center_distance - radius) /
                velocity;
            float second =
                (center_distance + radius) /
                velocity;

            if (first > second) {
                std::swap(first, second);
            }

            enter =
                std::max(
                    enter,
                    first);

            exit =
                std::min(
                    exit,
                    second);

            return enter <= exit;
        };

    if (!test_axis(
            separation.x,
            direction.x,
            cast_half.x +
                target.broad_half.x) ||
        !test_axis(
            separation.y,
            direction.y,
            cast_half.y +
                target.broad_half.y)) {
        return false;
    }

    if (!is_2d &&
        !test_axis(
            separation.z,
            direction.z,
            cast_half.z +
                target.broad_half.z)) {
        return false;
    }

    return exit >= 0.0f &&
        enter <= max_distance;
}

float point_aabb_distance_squared(
    core::Vec3 point,
    core::Vec3 center,
    core::Vec3 half,
    bool is_2d,
    core::Vec3* closest = nullptr) noexcept {

    core::Vec3 result{
        std::clamp(
            point.x,
            center.x - half.x,
            center.x + half.x),
        std::clamp(
            point.y,
            center.y - half.y,
            center.y + half.y),
        is_2d
            ? 0.0f
            : std::clamp(
                point.z,
                center.z - half.z,
                center.z + half.z)
    };

    if (closest) {
        *closest = result;
    }

    const core::Vec3 delta{
        point.x - result.x,
        point.y - result.y,
        is_2d
            ? 0.0f
            : point.z - result.z
    };

    return length_squared(
        delta,
        is_2d);
}

float segment_aabb_distance_squared(
    core::Vec3 a,
    core::Vec3 b,
    core::Vec3 center,
    core::Vec3 half,
    bool is_2d,
    core::Vec3* segment_point = nullptr,
    core::Vec3* box_point = nullptr) noexcept {

    if (is_2d) {
        a.z = 0.0f;
        b.z = 0.0f;
        center.z = 0.0f;
    }

    const core::Vec3 delta{
        b.x - a.x,
        b.y - a.y,
        is_2d
            ? 0.0f
            : b.z - a.z
    };

    const auto evaluate =
        [&](float t,
            core::Vec3* out_segment,
            core::Vec3* out_box) {

            const core::Vec3 point{
                a.x + delta.x * t,
                a.y + delta.y * t,
                is_2d
                    ? 0.0f
                    : a.z + delta.z * t
            };

            core::Vec3 closest{};

            const float squared =
                point_aabb_distance_squared(
                    point,
                    center,
                    half,
                    is_2d,
                    &closest);

            if (out_segment) {
                *out_segment = point;
            }

            if (out_box) {
                *out_box = closest;
            }

            return squared;
        };

    float low = 0.0f;
    float high = 1.0f;

    for (int iteration = 0;
         iteration < 40;
         ++iteration) {

        const float first =
            (low * 2.0f + high) /
            3.0f;

        const float second =
            (low + high * 2.0f) /
            3.0f;

        if (evaluate(
                first,
                nullptr,
                nullptr) <
            evaluate(
                second,
                nullptr,
                nullptr)) {
            high = second;
        } else {
            low = first;
        }
    }

    return evaluate(
        (low + high) * 0.5f,
        segment_point,
        box_point);
}

float cast_shape_gap(
    core::Vec3 origin,
    core::Vec3 cast_half,
    core::Vec3 direction,
    float time,
    const ColliderBounds& target,
    bool is_2d) noexcept {

    const core::Vec3 center{
        origin.x + direction.x * time,
        origin.y + direction.y * time,
        is_2d
            ? 0.0f
            : origin.z +
                direction.z * time
    };

    float squared = 0.0f;

    if (target.shape ==
        ColliderShape::Radial) {

        squared =
            point_aabb_distance_squared(
                target.center,
                center,
                cast_half,
                is_2d);
    } else {
        squared =
            segment_aabb_distance_squared(
                target.segment_a,
                target.segment_b,
                center,
                cast_half,
                is_2d);
    }

    return squared -
        target.radius *
            target.radius;
}

core::Vec3 cast_shape_normal(
    core::Vec3 origin,
    core::Vec3 cast_half,
    core::Vec3 direction,
    float time,
    const ColliderBounds& target,
    bool is_2d) noexcept {

    const core::Vec3 center{
        origin.x + direction.x * time,
        origin.y + direction.y * time,
        is_2d
            ? 0.0f
            : origin.z +
                direction.z * time
    };

    core::Vec3 target_point{};
    core::Vec3 box_point{};

    if (target.shape ==
        ColliderShape::Radial) {

        target_point =
            target.center;

        point_aabb_distance_squared(
            target.center,
            center,
            cast_half,
            is_2d,
            &box_point);
    } else {
        segment_aabb_distance_squared(
            target.segment_a,
            target.segment_b,
            center,
            cast_half,
            is_2d,
            &target_point,
            &box_point);
    }

    const core::Vec3 outward{
        box_point.x -
            target_point.x,
        box_point.y -
            target_point.y,
        is_2d
            ? 0.0f
            : box_point.z -
                target_point.z
    };

    if (length_squared(
            outward,
            is_2d) <=
        0.0000000001f) {

        return normalized_or_axis(
            {
                -direction.x,
                -direction.y,
                is_2d
                    ? 0.0f
                    : -direction.z
            },
            is_2d);
    }

    return normalized_or_axis(
        outward,
        is_2d);
}

bool swept_box_shape_hit(
    core::Vec3 origin,
    core::Vec3 cast_half,
    core::Vec3 direction,
    const ColliderBounds& target,
    float max_distance,
    bool is_2d,
    float& distance,
    core::Vec3& normal) noexcept {

    float broad_enter = 0.0f;
    float broad_exit =
        max_distance;

    if (!swept_aabb_interval(
            origin,
            cast_half,
            direction,
            target,
            max_distance,
            is_2d,
            broad_enter,
            broad_exit)) {
        return false;
    }

    broad_enter =
        std::clamp(
            broad_enter,
            0.0f,
            max_distance);

    broad_exit =
        std::clamp(
            broad_exit,
            0.0f,
            max_distance);

    if (broad_enter >
        broad_exit) {
        return false;
    }

    const float start_gap =
        cast_shape_gap(
            origin,
            cast_half,
            direction,
            broad_enter,
            target,
            is_2d);

    if (start_gap <= 0.0f) {
        distance =
            broad_enter;
        normal =
            cast_shape_normal(
                origin,
                cast_half,
                direction,
                distance,
                target,
                is_2d);
        return true;
    }

    float low =
        broad_enter;
    float high =
        broad_exit;

    for (int iteration = 0;
         iteration < 48;
         ++iteration) {

        const float first =
            (low * 2.0f + high) /
            3.0f;

        const float second =
            (low + high * 2.0f) /
            3.0f;

        if (cast_shape_gap(
                origin,
                cast_half,
                direction,
                first,
                target,
                is_2d) <
            cast_shape_gap(
                origin,
                cast_half,
                direction,
                second,
                target,
                is_2d)) {
            high = second;
        } else {
            low = first;
        }
    }

    const float minimum_time =
        (low + high) *
        0.5f;

    if (cast_shape_gap(
            origin,
            cast_half,
            direction,
            minimum_time,
            target,
            is_2d) >
        0.000001f) {
        return false;
    }

    low =
        broad_enter;
    high =
        minimum_time;

    for (int iteration = 0;
         iteration < 48;
         ++iteration) {

        const float middle =
            (low + high) *
            0.5f;

        if (cast_shape_gap(
                origin,
                cast_half,
                direction,
                middle,
                target,
                is_2d) <=
            0.0f) {
            high = middle;
        } else {
            low = middle;
        }
    }

    distance =
        std::clamp(
            high,
            0.0f,
            max_distance);

    normal =
        cast_shape_normal(
            origin,
            cast_half,
            direction,
            distance,
            target,
            is_2d);

    return true;
}

std::optional<RaycastHit>
box_cast_shape_collection(
    const std::vector<ColliderBounds>& bounds,
    core::Vec3 origin,
    core::Vec3 size,
    core::Vec3 direction,
    float max_distance,
    bool include_triggers,
    std::uint32_t layer_mask,
    bool is_2d) {

    if (size.x <= 0.0f ||
        size.y <= 0.0f ||
        (!is_2d &&
         size.z <= 0.0f) ||
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

        if (!swept_box_shape_hit(
                origin,
                cast_half,
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
        (!is_2d &&
         size.z <= 0.0f) ||
        !std::isfinite(
            max_distance) ||
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

        float distance = 0.0f;
        core::Vec3 normal{};

        if (!swept_box_hit(
                origin,
                cast_half,
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

} // namespace

std::optional<RaycastHit> box_cast(
    const core::World& world,
    core::Vec3 origin,
    core::Vec3 size,
    core::Vec3 direction,
    float max_distance,
    bool include_triggers,
    std::uint32_t layer_mask) {

    return nearest_hit(
        nearest_hit(
            box_cast_bounds<BoxCollider>(
                world,
                box_collider_type(),
                origin,
                size,
                direction,
                max_distance,
                include_triggers,
                layer_mask,
                false),
            box_cast_shape_collection(
                collect_radial_bounds<
                    SphereCollider>(
                        world,
                        sphere_collider_type(),
                        false),
                origin,
                size,
                direction,
                max_distance,
                include_triggers,
                layer_mask,
                false)),
        box_cast_shape_collection(
            collect_capsule_bounds(
                world),
            origin,
            size,
            direction,
            max_distance,
            include_triggers,
            layer_mask,
            false));
}

std::optional<RaycastHit> box_cast_2d(
    const core::World& world,
    core::Vec2 origin,
    core::Vec2 size,
    core::Vec2 direction,
    float max_distance,
    bool include_triggers,
    std::uint32_t layer_mask) {

    const core::Vec3 origin_3d{
        origin.x,
        origin.y,
        0.0f
    };

    const core::Vec3 size_3d{
        size.x,
        size.y,
        0.0f
    };

    const core::Vec3 direction_3d{
        direction.x,
        direction.y,
        0.0f
    };

    return nearest_hit(
        nearest_hit(
            box_cast_bounds<BoxCollider2D>(
                world,
                box_collider2d_type(),
                origin_3d,
                size_3d,
                direction_3d,
                max_distance,
                include_triggers,
                layer_mask,
                true),
            box_cast_shape_collection(
                collect_radial_bounds<
                    CircleCollider2D>(
                        world,
                        circle_collider2d_type(),
                        true),
                origin_3d,
                size_3d,
                direction_3d,
                max_distance,
                include_triggers,
                layer_mask,
                true)),
        box_cast_shape_collection(
            collect_capsule2d_bounds(
                world),
            origin_3d,
            size_3d,
            direction_3d,
            max_distance,
            include_triggers,
            layer_mask,
            true));
}

} // namespace nengine::physics
