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

        body2d->linear_velocity.z =
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

    return result;
}

} // namespace nengine::physics
