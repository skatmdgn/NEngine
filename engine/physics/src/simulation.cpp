#include "nengine/physics/simulation.hpp"

#include <cmath>

#include "nengine/physics/components.hpp"

namespace nengine::physics {
namespace {

void add_scaled(
    core::Vec3& destination,
    core::Vec3 value,
    float scale) noexcept {

    destination.x +=
        value.x * scale;
    destination.y +=
        value.y * scale;
    destination.z +=
        value.z * scale;
}

float speed_squared(
    core::Vec3 value,
    bool is_2d) noexcept {

    return value.x * value.x +
        value.y * value.y +
        (is_2d
            ? 0.0f
            : value.z * value.z);
}

float angular_speed_squared(
    core::Vec3 value,
    bool is_2d) noexcept {

    if (is_2d) {
        return value.z * value.z;
    }

    return value.x * value.x +
        value.y * value.y +
        value.z * value.z;
}

float motion_speed_squared(
    core::Vec3 linear,
    core::Vec3 angular,
    bool is_2d) noexcept {

    return speed_squared(
               linear,
               is_2d) +
        angular_speed_squared(
               angular,
               is_2d);
}

core::Quat normalized_quat(
    core::Quat value) noexcept {

    const float squared =
        value.x * value.x +
        value.y * value.y +
        value.z * value.z +
        value.w * value.w;

    if (squared <= 0.0000000001f) {
        return {};
    }

    const float inverse =
        1.0f /
        std::sqrt(squared);

    return {
        value.x * inverse,
        value.y * inverse,
        value.z * inverse,
        value.w * inverse
    };
}

core::Quat multiplied(
    core::Quat a,
    core::Quat b) noexcept {

    return {
        a.w * b.x +
            a.x * b.w +
            a.y * b.z -
            a.z * b.y,
        a.w * b.y -
            a.x * b.z +
            a.y * b.w +
            a.z * b.x,
        a.w * b.z +
            a.x * b.y -
            a.y * b.x +
            a.z * b.w,
        a.w * b.w -
            a.x * b.x -
            a.y * b.y -
            a.z * b.z
    };
}

void integrate_rotation(
    core::Quat& rotation,
    core::Vec3 angular_velocity,
    float delta_seconds,
    bool is_2d) noexcept {

    if (is_2d) {
        angular_velocity.x = 0.0f;
        angular_velocity.y = 0.0f;
    }

    const float magnitude_squared =
        angular_velocity.x *
            angular_velocity.x +
        angular_velocity.y *
            angular_velocity.y +
        angular_velocity.z *
            angular_velocity.z;

    if (magnitude_squared <=
        0.0000000001f) {
        return;
    }

    const float magnitude =
        std::sqrt(
            magnitude_squared);

    const float angle =
        magnitude *
        delta_seconds;

    const float half_angle =
        angle * 0.5f;

    const float axis_scale =
        std::sin(half_angle) /
        magnitude;

    const core::Quat delta{
        angular_velocity.x *
            axis_scale,
        angular_velocity.y *
            axis_scale,
        angular_velocity.z *
            axis_scale,
        std::cos(half_angle)
    };

    rotation =
        normalized_quat(
            multiplied(
                delta,
                rotation));
}

bool has_solid_contact(
    const std::vector<BoxOverlap>& overlaps,
    core::Entity entity,
    bool is_2d) noexcept {

    for (const auto& overlap :
         overlaps) {

        if (overlap.is_2d != is_2d ||
            overlap.is_trigger ||
            overlap.penetration <= 0.0f) {
            continue;
        }

        if (overlap.first == entity ||
            overlap.second == entity) {
            return true;
        }
    }

    return false;
}

template <typename Body>
void update_body_sleep(
    Body& body,
    bool supported,
    float delta_seconds,
    bool is_2d) noexcept {

    if (!body.enabled ||
        body.is_kinematic ||
        !body.allow_sleep) {

        body.sleeping = false;
        body.sleep_timer = 0.0f;
        return;
    }

    const float threshold_squared =
        body.sleep_threshold *
        body.sleep_threshold;

    const float body_speed_squared =
        motion_speed_squared(
            body.linear_velocity,
            body.angular_velocity,
            is_2d);

    if (!supported ||
        body_speed_squared >
            threshold_squared) {

        body.sleeping = false;
        body.sleep_timer = 0.0f;
        return;
    }

    constexpr float sleep_delay_seconds =
        0.5f;

    if (!body.sleeping) {
        body.sleep_timer +=
            delta_seconds;

        if (body.sleep_timer >=
            sleep_delay_seconds) {
            body.sleeping = true;
        }
    }

    if (body.sleeping) {
        body.linear_velocity = {};
        body.angular_velocity = {};
    }
}

void update_sleep_states(
    core::World& world,
    const std::vector<BoxOverlap>& overlaps,
    float delta_seconds) noexcept {

    if (!std::isfinite(delta_seconds) ||
        delta_seconds <= 0.0f) {
        return;
    }

    for (const auto entity :
         world.entities()) {

        if (!world.active(entity)) {
            continue;
        }

        if (auto* body =
                world.get_component<Rigidbody>(
                    entity,
                    rigidbody_type())) {

            update_body_sleep(
                *body,
                has_solid_contact(
                    overlaps,
                    entity,
                    false),
                delta_seconds,
                false);

            continue;
        }

        if (auto* body2d =
                world.get_component<Rigidbody2D>(
                    entity,
                    rigidbody2d_type())) {

            body2d->linear_velocity.z =
                0.0f;
            body2d->angular_velocity.x =
                0.0f;
            body2d->angular_velocity.y =
                0.0f;

            update_body_sleep(
                *body2d,
                has_solid_contact(
                    overlaps,
                    entity,
                    true),
                delta_seconds,
                true);
        }
    }
}

} // namespace

PhysicsStepStats step_rigidbodies(
    core::World& world,
    float delta_seconds,
    core::Vec3 gravity) noexcept {

    PhysicsStepStats stats;

    if (!std::isfinite(delta_seconds) ||
        delta_seconds <= 0.0f) {
        return stats;
    }

    for (const auto entity :
         world.entities()) {

        if (!world.active(entity)) {
            continue;
        }

        auto* transform =
            world.transform(entity);

        if (!transform) {
            continue;
        }

        auto* body =
            world.get_component<Rigidbody>(
                entity,
                rigidbody_type());

        if (body &&
            body->enabled &&
            !body->is_kinematic) {

            if (body->sleeping) {
                const float threshold_squared =
                    body->sleep_threshold *
                    body->sleep_threshold;

                if (!body->allow_sleep ||
                    motion_speed_squared(
                        body->linear_velocity,
                        body->angular_velocity,
                        false) >
                        threshold_squared) {

                    body->sleeping = false;
                    body->sleep_timer = 0.0f;
                } else {
                    continue;
                }
            }

            if (body->use_gravity) {
                add_scaled(
                    body->linear_velocity,
                    gravity,
                    body->gravity_scale *
                        delta_seconds);

                ++stats.gravity_applied;
            }

            add_scaled(
                transform->local_position,
                body->linear_velocity,
                delta_seconds);

            integrate_rotation(
                transform->local_rotation,
                body->angular_velocity,
                delta_seconds,
                false);

            ++stats.integrated_3d;
            continue;
        }

        auto* body2d =
            world.get_component<Rigidbody2D>(
                entity,
                rigidbody2d_type());

        if (!body2d ||
            !body2d->enabled ||
            body2d->is_kinematic) {
            continue;
        }

        body2d->linear_velocity.z =
            0.0f;
        body2d->angular_velocity.x =
            0.0f;
        body2d->angular_velocity.y =
            0.0f;

        if (body2d->sleeping) {
            const float threshold_squared =
                body2d->sleep_threshold *
                body2d->sleep_threshold;

            if (!body2d->allow_sleep ||
                motion_speed_squared(
                    body2d->linear_velocity,
                    body2d->angular_velocity,
                    true) >
                    threshold_squared) {

                body2d->sleeping = false;
                body2d->sleep_timer = 0.0f;
            } else {
                continue;
            }
        }

        if (body2d->use_gravity) {
            body2d->linear_velocity.x +=
                gravity.x *
                body2d->gravity_scale *
                delta_seconds;

            body2d->linear_velocity.y +=
                gravity.y *
                body2d->gravity_scale *
                delta_seconds;

            ++stats.gravity_applied;
        }

        transform->local_position.x +=
            body2d->linear_velocity.x *
            delta_seconds;

        transform->local_position.y +=
            body2d->linear_velocity.y *
            delta_seconds;

        integrate_rotation(
            transform->local_rotation,
            body2d->angular_velocity,
            delta_seconds,
            true);

        body2d->linear_velocity.z =
            0.0f;
        body2d->angular_velocity.x =
            0.0f;
        body2d->angular_velocity.y =
            0.0f;

        ++stats.integrated_2d;
    }

    return stats;
}

PhysicsFrameResult step_physics(
    core::World& world,
    float delta_seconds,
    core::Vec3 gravity) {

    PhysicsFrameResult result;

    result.integration =
        step_rigidbodies(
            world,
            delta_seconds,
            gravity);

    result.collisions =
        detect_box_overlaps(
            world);

    result.resolution =
        resolve_box_contacts_3d(
            world,
            result.collisions.overlaps);

    const auto resolution_2d =
        resolve_box_contacts_2d(
            world,
            result.collisions.overlaps);

    result.resolution.resolved_2d +=
        resolution_2d.resolved_2d;

    update_sleep_states(
        world,
        result.collisions.overlaps,
        delta_seconds);

    return result;
}

} // namespace nengine::physics
