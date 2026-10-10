#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>

#include "nengine/core/component_registry.hpp"
#include "nengine/core/component_serialization.hpp"
#include "nengine/core/world.hpp"
#include "nengine/physics/components.hpp"
#include "nengine/physics/collision.hpp"
#include "nengine/physics/contact_events.hpp"
#include "nengine/physics/registration.hpp"
#include "nengine/physics/simulation.hpp"

namespace {

int failures = 0;

void check(
    bool condition,
    const char* message) {

    if (!condition) {
        ++failures;
        std::cerr
            << "FAIL: "
            << message
            << '\n';
    }
}

void set_property(
    nengine::core::SerializedComponentData& data,
    const char* name,
    nengine::core::PropertyValue value) {

    for (auto& property :
         data.properties) {
        if (property.name == name) {
            property.value =
                std::move(value);
            return;
        }
    }
}

} // namespace

int main() {
    using namespace nengine;

    core::ComponentRegistry metadata;
    core::ComponentSerializationRegistry
        serialization;

    check(
        physics::register_component_metadata(
            metadata),
        "physics component metadata registers");

    check(
        physics::register_component_serializers(
            serialization),
        "physics component serializers register");

    check(
        metadata.find(
            physics::rigidbody_type()) != nullptr &&
        metadata.find(
            physics::box_collider_type()) != nullptr &&
        metadata.find(
            physics::sphere_collider_type()) != nullptr &&
        metadata.find(
            physics::rigidbody2d_type()) != nullptr &&
        metadata.find(
            physics::box_collider2d_type()) != nullptr &&
        metadata.find(
            physics::circle_collider2d_type()) != nullptr,
        "3D and 2D rigidbody box sphere and circle component descriptors are discoverable");

    core::World world;
    const auto entity =
        world.create(
            "Physics Body");

    auto* body =
        world.add_component<
            physics::Rigidbody>(
                entity,
                physics::rigidbody_type());

    auto* collider =
        world.add_component<
            physics::BoxCollider>(
                entity,
                physics::box_collider_type());

    auto* sphere =
        world.add_component<
            physics::SphereCollider>(
                entity,
                physics::sphere_collider_type());

    auto* body2d =
        world.add_component<
            physics::Rigidbody2D>(
                entity,
                physics::rigidbody2d_type());

    auto* collider2d =
        world.add_component<
            physics::BoxCollider2D>(
                entity,
                physics::box_collider2d_type());

    auto* circle2d =
        world.add_component<
            physics::CircleCollider2D>(
                entity,
                physics::circle_collider2d_type());

    check(
        body &&
        collider &&
        sphere &&
        body2d &&
        collider2d &&
        circle2d,
        "physics components attach to World entities");

    if (body) {
        body->mass = 2.5f;
        body->gravity_scale = 0.75f;
        body->linear_velocity =
            {1.0f, 2.0f, 3.0f};
    }

    if (collider) {
        collider->is_trigger = true;
        collider->layer = 3u;
        collider->collision_mask =
            0x000000a5u;
        collider->friction = 0.25f;
        collider->restitution = 0.75f;
        collider->center =
            {0.25f, 0.5f, 0.75f};
        collider->size =
            {2.0f, 3.0f, 4.0f};
    }

    if (sphere) {
        sphere->is_trigger = true;
        sphere->layer = 9u;
        sphere->collision_mask =
            0x00000f0fu;
        sphere->friction = 0.8f;
        sphere->restitution = 0.3f;
        sphere->center =
            {1.0f, 2.0f, 3.0f};
        sphere->radius = 1.25f;
    }

    if (body2d) {
        body2d->use_gravity = false;
        body2d->mass = 3.0f;
        body2d->linear_velocity =
            {4.0f, 5.0f, 0.0f};
    }

    if (collider2d) {
        collider2d->layer = 7u;
        collider2d->collision_mask =
            0x0000ff00u;
        collider2d->friction = 0.6f;
        collider2d->restitution = 0.2f;
        collider2d->size =
            {6.0f, 7.0f, 0.0f};
    }

    if (circle2d) {
        circle2d->friction = 0.4f;
        circle2d->restitution = 0.9f;
    }

    const auto captured_body =
        serialization.capture(
            world,
            entity,
            physics::rigidbody_type());

    const auto captured_collider =
        serialization.capture(
            world,
            entity,
            physics::box_collider_type());

    const auto captured_sphere =
        serialization.capture(
            world,
            entity,
            physics::sphere_collider_type());

    const auto captured_body2d =
        serialization.capture(
            world,
            entity,
            physics::rigidbody2d_type());

    const auto captured_collider2d =
        serialization.capture(
            world,
            entity,
            physics::box_collider2d_type());

    const auto captured_circle2d =
        serialization.capture(
            world,
            entity,
            physics::circle_collider2d_type());

    check(
        captured_body &&
        captured_collider &&
        captured_sphere &&
        captured_body2d &&
        captured_collider2d &&
        captured_circle2d,
        "physics codecs capture all component data");

    core::World restored;
    const auto restored_entity =
        restored.create(
            "Restored Physics Body");

    std::string error;

    check(
        captured_body &&
        serialization.restore(
            restored,
            restored_entity,
            *captured_body,
            &error) &&
        captured_collider &&
        serialization.restore(
            restored,
            restored_entity,
            *captured_collider,
            &error) &&
        captured_sphere &&
        serialization.restore(
            restored,
            restored_entity,
            *captured_sphere,
            &error) &&
        captured_body2d &&
        serialization.restore(
            restored,
            restored_entity,
            *captured_body2d,
            &error) &&
        captured_collider2d &&
        serialization.restore(
            restored,
            restored_entity,
            *captured_collider2d,
            &error) &&
        captured_circle2d &&
        serialization.restore(
            restored,
            restored_entity,
            *captured_circle2d,
            &error),
        "physics codecs restore 3D and 2D component data");

    const auto* restored_body =
        restored.get_component<
            physics::Rigidbody>(
                restored_entity,
                physics::rigidbody_type());

    const auto* restored_collider =
        restored.get_component<
            physics::BoxCollider>(
                restored_entity,
                physics::box_collider_type());

    const auto* restored_sphere =
        restored.get_component<
            physics::SphereCollider>(
                restored_entity,
                physics::sphere_collider_type());

    const auto* restored_body2d =
        restored.get_component<
            physics::Rigidbody2D>(
                restored_entity,
                physics::rigidbody2d_type());

    const auto* restored_collider2d =
        restored.get_component<
            physics::BoxCollider2D>(
                restored_entity,
                physics::box_collider2d_type());

    const auto* restored_circle2d =
        restored.get_component<
            physics::CircleCollider2D>(
                restored_entity,
                physics::circle_collider2d_type());

    check(
        restored_body &&
        std::abs(
            restored_body->mass -
            2.5f) < 0.0001f &&
        restored_body->linear_velocity ==
            core::Vec3{1.0f, 2.0f, 3.0f} &&
        restored_collider &&
        restored_collider->is_trigger &&
        restored_collider->layer == 3u &&
        restored_collider->collision_mask ==
            0x000000a5u &&
        std::abs(
            restored_collider->friction -
            0.25f) < 0.0001f &&
        std::abs(
            restored_collider->restitution -
            0.75f) < 0.0001f &&
        restored_collider->size ==
            core::Vec3{2.0f, 3.0f, 4.0f} &&
        restored_sphere &&
        restored_sphere->is_trigger &&
        restored_sphere->layer == 9u &&
        restored_sphere->collision_mask ==
            0x00000f0fu &&
        std::abs(
            restored_sphere->friction -
            0.8f) < 0.0001f &&
        std::abs(
            restored_sphere->restitution -
            0.3f) < 0.0001f &&
        restored_sphere->center ==
            core::Vec3{1.0f, 2.0f, 3.0f} &&
        std::abs(
            restored_sphere->radius -
            1.25f) < 0.0001f &&
        restored_body2d &&
        !restored_body2d->use_gravity &&
        restored_body2d->linear_velocity ==
            core::Vec3{4.0f, 5.0f, 0.0f} &&
        restored_collider2d &&
        restored_collider2d->layer == 7u &&
        restored_collider2d->collision_mask ==
            0x0000ff00u &&
        std::abs(
            restored_collider2d->friction -
            0.6f) < 0.0001f &&
        std::abs(
            restored_collider2d->restitution -
            0.2f) < 0.0001f &&
        restored_collider2d->size ==
            core::Vec3{6.0f, 7.0f, 0.0f} &&
        restored_circle2d &&
        restored_circle2d->radius ==
            0.5f &&
        std::abs(
            restored_circle2d->friction -
            0.4f) < 0.0001f &&
        std::abs(
            restored_circle2d->restitution -
            0.9f) < 0.0001f,
        "physics Scene roundtrip preserves configured values");

    if (captured_collider) {
        auto legacy =
            *captured_collider;

        legacy.properties.erase(
            std::remove_if(
                legacy.properties.begin(),
                legacy.properties.end(),
                [](const auto& property) {
                    return property.name == "Layer" ||
                           property.name == "Collision Mask" ||
                           property.name == "Friction" ||
                           property.name == "Restitution";
                }),
            legacy.properties.end());

        core::World legacy_world;
        const auto legacy_entity =
            legacy_world.create(
                "Legacy Collider");

        check(
            serialization.restore(
                legacy_world,
                legacy_entity,
                legacy,
                &error),
            "BoxCollider restore remains compatible with pre-layer Scene data");

        const auto* legacy_collider =
            legacy_world.get_component<
                physics::BoxCollider>(
                    legacy_entity,
                    physics::box_collider_type());

        check(
            legacy_collider &&
            legacy_collider->layer == 0u &&
            legacy_collider->collision_mask ==
                0xffffffffu &&
            std::abs(
                legacy_collider->friction -
                0.5f) < 0.0001f &&
            std::abs(
                legacy_collider->restitution) <
                0.0001f,
            "legacy BoxCollider data defaults layer filtering and contact material values");
    }

    if (captured_body) {
        auto invalid =
            *captured_body;

        set_property(
            invalid,
            "Mass",
            core::PropertyValue{-1.0});

        check(
            !serialization.restore(
                restored,
                restored_entity,
                invalid,
                &error),
            "Rigidbody codec rejects non-positive mass");
    }

    if (captured_collider) {
        auto invalid =
            *captured_collider;

        set_property(
            invalid,
            "Size",
            core::PropertyValue{
                core::Vec3{
                    1.0f,
                    0.0f,
                    1.0f}});

        check(
            !serialization.restore(
                restored,
                restored_entity,
                invalid,
                &error),
            "BoxCollider codec rejects non-positive dimensions");
    }

    if (captured_collider) {
        auto invalid =
            *captured_collider;

        set_property(
            invalid,
            "Restitution",
            core::PropertyValue{1.5});

        check(
            !serialization.restore(
                restored,
                restored_entity,
                invalid,
                &error),
            "BoxCollider codec rejects contact material values outside zero to one");
    }

    if (captured_sphere) {
        auto invalid =
            *captured_sphere;

        set_property(
            invalid,
            "Radius",
            core::PropertyValue{0.0});

        check(
            !serialization.restore(
                restored,
                restored_entity,
                invalid,
                &error),
            "SphereCollider codec rejects non-positive radius");
    }

    if (captured_circle2d) {
        auto invalid =
            *captured_circle2d;

        set_property(
            invalid,
            "Radius",
            core::PropertyValue{-1.0});

        check(
            !serialization.restore(
                restored,
                restored_entity,
                invalid,
                &error),
            "CircleCollider2D codec rejects non-positive radius");
    }

    core::World simulation_world;

    const auto body_entity =
        simulation_world.create(
            "Dynamic 3D");

    auto* simulated_body =
        simulation_world.add_component<
            physics::Rigidbody>(
                body_entity,
                physics::rigidbody_type());

    if (simulated_body) {
        simulated_body->linear_velocity =
            {2.0f, 0.0f, 0.0f};
    }

    const auto body2d_entity =
        simulation_world.create(
            "Dynamic 2D");

    auto* simulated_body2d =
        simulation_world.add_component<
            physics::Rigidbody2D>(
                body2d_entity,
                physics::rigidbody2d_type());

    if (simulated_body2d) {
        simulated_body2d->use_gravity =
            false;
        simulated_body2d->linear_velocity =
            {3.0f, 4.0f, 0.0f};
    }

    const auto kinematic_entity =
        simulation_world.create(
            "Kinematic");

    auto* kinematic_body =
        simulation_world.add_component<
            physics::Rigidbody>(
                kinematic_entity,
                physics::rigidbody_type());

    if (kinematic_body) {
        kinematic_body->is_kinematic =
            true;
        kinematic_body->linear_velocity =
            {100.0f, 100.0f, 100.0f};
    }

    const auto step =
        physics::step_rigidbodies(
            simulation_world,
            0.5f);

    simulated_body =
        simulation_world.get_component<
            physics::Rigidbody>(
                body_entity,
                physics::rigidbody_type());

    simulated_body2d =
        simulation_world.get_component<
            physics::Rigidbody2D>(
                body2d_entity,
                physics::rigidbody2d_type());

    kinematic_body =
        simulation_world.get_component<
            physics::Rigidbody>(
                kinematic_entity,
                physics::rigidbody_type());

    const auto* body_transform =
        simulation_world.transform(
            body_entity);

    const auto* body2d_transform =
        simulation_world.transform(
            body2d_entity);

    const auto* kinematic_transform =
        simulation_world.transform(
            kinematic_entity);

    check(
        step.integrated_3d == 1u &&
        step.integrated_2d == 1u &&
        step.gravity_applied == 1u &&
        simulated_body &&
        std::abs(
            simulated_body
                ->linear_velocity.y +
            4.905f) < 0.0001f &&
        body_transform &&
        std::abs(
            body_transform
                ->local_position.x -
            1.0f) < 0.0001f &&
        std::abs(
            body_transform
                ->local_position.y +
            2.4525f) < 0.0001f &&
        simulated_body2d &&
        simulated_body2d
            ->linear_velocity ==
            core::Vec3{
                3.0f,
                4.0f,
                0.0f} &&
        body2d_transform &&
        body2d_transform
            ->local_position ==
            core::Vec3{
                1.5f,
                2.0f,
                0.0f} &&
        kinematic_body &&
        kinematic_body->is_kinematic &&
        kinematic_transform &&
        kinematic_transform
            ->local_position ==
            core::Vec3{},
        "fixed-step rigidbody foundation integrates gravity velocity and Transform while skipping kinematic bodies");

    const auto invalid_step =
        physics::step_rigidbodies(
            simulation_world,
            -1.0f);

    check(
        invalid_step.integrated_3d == 0u &&
        invalid_step.integrated_2d == 0u,
        "physics fixed-step foundation ignores invalid negative delta");

    core::World collision_world;

    const auto box_a =
        collision_world.create(
            "Box A");
    const auto box_b =
        collision_world.create(
            "Box B");
    const auto box_c =
        collision_world.create(
            "Box C");

    collision_world.add_component<
        physics::BoxCollider>(
            box_a,
            physics::box_collider_type());

    auto* box_b_collider =
        collision_world.add_component<
            physics::BoxCollider>(
                box_b,
                physics::box_collider_type());

    collision_world.add_component<
        physics::BoxCollider>(
            box_c,
            physics::box_collider_type());

    collision_world.transform(
        box_b)->local_position =
            {0.75f, 0.0f, 0.0f};

    collision_world.transform(
        box_c)->local_position =
            {4.0f, 0.0f, 0.0f};

    box_b_collider =
        collision_world.get_component<
            physics::BoxCollider>(
                box_b,
                physics::box_collider_type());

    if (box_b_collider) {
        box_b_collider->is_trigger =
            true;
    }

    const auto box2d_a =
        collision_world.create(
            "Box2D A");
    const auto box2d_b =
        collision_world.create(
            "Box2D B");

    collision_world.add_component<
        physics::BoxCollider2D>(
            box2d_a,
            physics::box_collider2d_type());

    collision_world.add_component<
        physics::BoxCollider2D>(
            box2d_b,
            physics::box_collider2d_type());

    collision_world.transform(
        box2d_b)->local_position =
            {0.0f, 0.6f, 10.0f};

    const auto detection =
        physics::detect_box_overlaps(
            collision_world);

    check(
        detection.tested_pairs_3d ==
            1u &&
        detection.tested_pairs_2d ==
            1u &&
        detection.overlaps.size() ==
            2u,
        "sweep-and-prune broad phase culls separated 3D pairs while preserving 2D overlap scans");

    bool saw_3d_trigger = false;
    bool saw_2d_contact = false;

    for (const auto& overlap :
         detection.overlaps) {

        if (!overlap.is_2d &&
            overlap.is_trigger &&
            overlap.first == box_a &&
            overlap.second == box_b) {

            saw_3d_trigger =
                overlap.normal ==
                    core::Vec3{
                        1.0f,
                        0.0f,
                        0.0f} &&
                std::abs(
                    overlap.penetration -
                    0.25f) <
                    0.0001f;
        }

        if (overlap.is_2d &&
            !overlap.is_trigger &&
            overlap.first == box2d_a &&
            overlap.second == box2d_b) {

            saw_2d_contact =
                overlap.normal ==
                    core::Vec3{
                        0.0f,
                        1.0f,
                        0.0f} &&
                std::abs(
                    overlap.penetration -
                    0.4f) <
                    0.0001f;
        }
    }

    check(
        saw_3d_trigger &&
        saw_2d_contact,
        "AABB overlap records minimum penetration axis and trigger semantics");

    core::World sparse_broad_phase_world;

    for (int index = 0;
         index < 32;
         ++index) {

        const auto entity =
            sparse_broad_phase_world.create(
                "Sparse Box");

        sparse_broad_phase_world
            .add_component<
                physics::BoxCollider>(
                    entity,
                    physics::box_collider_type());

        sparse_broad_phase_world
            .transform(entity)
            ->local_position = {
                static_cast<float>(
                    index * 4),
                0.0f,
                0.0f
            };
    }

    const auto sparse_detection =
        physics::detect_box_overlaps(
            sparse_broad_phase_world);

    check(
        sparse_detection
            .tested_pairs_3d == 0u &&
        sparse_detection
            .overlaps
            .empty(),
        "sweep-and-prune broad phase avoids quadratic narrow-phase tests for sparse colliders");

    core::World layer_world;

    const auto layer_a =
        layer_world.create("Layer A");
    const auto layer_b =
        layer_world.create("Layer B");
    const auto layer_c =
        layer_world.create("Layer C");

    auto* layer_a_box =
        layer_world.add_component<
            physics::BoxCollider>(
                layer_a,
                physics::box_collider_type());

    auto* layer_b_box =
        layer_world.add_component<
            physics::BoxCollider>(
                layer_b,
                physics::box_collider_type());

    layer_world.add_component<
        physics::BoxCollider>(
            layer_c,
            physics::box_collider_type());

    layer_a_box =
        layer_world.get_component<
            physics::BoxCollider>(
                layer_a,
                physics::box_collider_type());

    layer_b_box =
        layer_world.get_component<
            physics::BoxCollider>(
                layer_b,
                physics::box_collider_type());

    auto* layer_c_box =
        layer_world.get_component<
            physics::BoxCollider>(
                layer_c,
                physics::box_collider_type());

    if (layer_a_box) {
        layer_a_box->layer = 0u;
        layer_a_box->collision_mask =
            (1u << 1u);
    }

    if (layer_b_box) {
        layer_b_box->layer = 1u;
        layer_b_box->collision_mask =
            (1u << 0u);
    }

    if (layer_c_box) {
        layer_c_box->layer = 2u;
        layer_c_box->collision_mask =
            0xffffffffu;
    }

    const auto layer_detection =
        physics::detect_box_overlaps(
            layer_world);

    check(
        layer_detection
            .tested_pairs_3d == 1u &&
        layer_detection
            .overlaps.size() == 1u &&
        layer_detection
            .overlaps.front().first ==
            layer_a &&
        layer_detection
            .overlaps.front().second ==
            layer_b,
        "collider collision masks require mutual layer permission before narrow phase");

    const auto layer_query =
        physics::overlap_box(
            layer_world,
            {},
            {2.0f, 2.0f, 2.0f},
            true,
            (1u << 1u));

    check(
        layer_query.size() == 1u &&
        layer_query.front() ==
            layer_b,
        "OverlapBox layer mask filters candidate collider layers");

    const auto layer_ray =
        physics::raycast(
            layer_world,
            {-2.0f, 0.0f, 0.0f},
            {1.0f, 0.0f, 0.0f},
            10.0f,
            true,
            (1u << 1u));

    check(
        layer_ray &&
        layer_ray->entity ==
            layer_b &&
        layer_ray->layer == 1u,
        "Raycast layer mask filters hits and returns the hit layer");

    physics::ContactTracker
        contact_tracker;

    const auto enter_events =
        contact_tracker.update(
            detection.overlaps);

    std::size_t enter_count = 0u;

    for (const auto& event :
         enter_events) {
        if (event.phase ==
            physics::ContactPhase::Enter) {
            ++enter_count;
        }
    }

    check(
        enter_count == 2u &&
        contact_tracker.active_pair_count() ==
            2u,
        "contact tracker emits Enter for newly overlapping 3D/2D pairs");

    const auto stay_events =
        contact_tracker.update(
            detection.overlaps);

    std::size_t stay_count = 0u;

    for (const auto& event :
         stay_events) {
        if (event.phase ==
            physics::ContactPhase::Stay) {
            ++stay_count;
        }
    }

    check(
        stay_count == 2u,
        "contact tracker emits Stay for persistent overlap pairs");

    collision_world.transform(
        box_b)->local_position =
            {5.0f, 0.0f, 0.0f};

    const auto reduced_detection =
        physics::detect_box_overlaps(
            collision_world);

    const auto exit_events =
        contact_tracker.update(
            reduced_detection.overlaps);

    bool saw_trigger_exit = false;
    bool saw_2d_stay = false;

    for (const auto& event :
         exit_events) {

        if (event.phase ==
                physics::ContactPhase::Exit &&
            event.is_trigger &&
            !event.is_2d) {
            saw_trigger_exit = true;
        }

        if (event.phase ==
                physics::ContactPhase::Stay &&
            !event.is_trigger &&
            event.is_2d) {
            saw_2d_stay = true;
        }
    }

    check(
        saw_trigger_exit &&
        saw_2d_stay &&
        contact_tracker.active_pair_count() ==
            1u,
        "contact tracker separates trigger Exit from persistent 2D collision Stay");

    contact_tracker.clear();

    check(
        contact_tracker.active_pair_count() ==
            0u,
        "contact tracker clear resets active overlap state");

    collision_world.transform(
        box_b)->local_position =
            {0.75f, 0.0f, 0.0f};

    const auto query_3d_all =
        physics::overlap_box(
            collision_world,
            {0.0f, 0.0f, 0.0f},
            {2.0f, 2.0f, 2.0f});

    const auto query_3d_solid =
        physics::overlap_box(
            collision_world,
            {0.0f, 0.0f, 0.0f},
            {2.0f, 2.0f, 2.0f},
            false);

    const auto query_2d =
        physics::overlap_box_2d(
            collision_world,
            {0.0f, 0.0f},
            {2.0f, 2.0f});

    check(
        query_3d_all.size() == 2u &&
        query_3d_solid.size() == 1u &&
        query_3d_solid.front() ==
            box_a &&
        query_2d.size() == 2u,
        "OverlapBox queries return active 3D/2D colliders and can exclude triggers");

    check(
        physics::overlap_box(
            collision_world,
            {},
            {0.0f, 1.0f, 1.0f})
            .empty() &&
        physics::overlap_box_2d(
            collision_world,
            {},
            {-1.0f, 1.0f})
            .empty(),
        "OverlapBox queries reject non-positive query dimensions");

    const auto ray_hit =
        physics::raycast(
            collision_world,
            {-3.0f, 0.0f, 0.0f},
            {1.0f, 0.0f, 0.0f},
            10.0f,
            false);

    check(
        ray_hit &&
        ray_hit->entity == box_a &&
        !ray_hit->is_trigger &&
        !ray_hit->is_2d &&
        std::abs(
            ray_hit->distance -
            2.5f) < 0.0001f &&
        ray_hit->normal ==
            core::Vec3{
                -1.0f,
                0.0f,
                0.0f} &&
        ray_hit->point ==
            core::Vec3{
                -0.5f,
                0.0f,
                0.0f},
        "3D Raycast returns nearest non-trigger BoxCollider hit point normal and distance");

    const auto ray_hit_2d =
        physics::raycast_2d(
            collision_world,
            {0.0f, -3.0f},
            {0.0f, 1.0f},
            10.0f);

    check(
        ray_hit_2d &&
        ray_hit_2d->entity ==
            box2d_a &&
        ray_hit_2d->is_2d &&
        std::abs(
            ray_hit_2d->distance -
            2.5f) < 0.0001f &&
        ray_hit_2d->normal ==
            core::Vec3{
                0.0f,
                -1.0f,
                0.0f},
        "2D Raycast returns nearest BoxCollider2D hit on XY");

    check(
        !physics::raycast(
            collision_world,
            {},
            {},
            10.0f) &&
        !physics::raycast_2d(
            collision_world,
            {},
            {1.0f, 0.0f},
            -1.0f),
        "Raycast queries reject zero direction and invalid distance");

    core::World cast_world;

    const auto cast_target =
        cast_world.create(
            "Cast Target");

    auto* cast_target_box =
        cast_world.add_component<
            physics::BoxCollider>(
                cast_target,
                physics::box_collider_type());

    if (cast_target_box) {
        cast_target_box->layer = 4u;
    }

    const auto cast_target_2d =
        cast_world.create(
            "Cast Target 2D");

    auto* cast_target_box_2d =
        cast_world.add_component<
            physics::BoxCollider2D>(
                cast_target_2d,
                physics::box_collider2d_type());

    if (cast_target_box_2d) {
        cast_target_box_2d->layer = 6u;
    }

    const auto cast_hit =
        physics::box_cast(
            cast_world,
            {-3.0f, 0.0f, 0.0f},
            {1.0f, 1.0f, 1.0f},
            {1.0f, 0.0f, 0.0f},
            10.0f,
            true,
            (1u << 4u));

    check(
        cast_hit &&
        cast_hit->entity ==
            cast_target &&
        cast_hit->layer == 4u &&
        !cast_hit->is_2d &&
        std::abs(
            cast_hit->distance -
            2.0f) < 0.0001f &&
        cast_hit->normal ==
            core::Vec3{
                -1.0f,
                0.0f,
                0.0f} &&
        cast_hit->point ==
            core::Vec3{
                -1.0f,
                0.0f,
                0.0f},
        "3D BoxCast uses Minkowski-expanded AABB and returns cast-center time of impact");

    const auto cast_hit_2d =
        physics::box_cast_2d(
            cast_world,
            {0.0f, -3.0f},
            {1.0f, 1.0f},
            {0.0f, 1.0f},
            10.0f,
            true,
            (1u << 6u));

    check(
        cast_hit_2d &&
        cast_hit_2d->entity ==
            cast_target_2d &&
        cast_hit_2d->layer == 6u &&
        cast_hit_2d->is_2d &&
        std::abs(
            cast_hit_2d->distance -
            2.0f) < 0.0001f &&
        cast_hit_2d->normal ==
            core::Vec3{
                0.0f,
                -1.0f,
                0.0f} &&
        cast_hit_2d->point ==
            core::Vec3{
                0.0f,
                -1.0f,
                0.0f},
        "2D BoxCast sweeps axis-aligned boxes on XY with layer filtering");

    check(
        !physics::box_cast(
            cast_world,
            {},
            {0.0f, 1.0f, 1.0f},
            {1.0f, 0.0f, 0.0f},
            10.0f) &&
        !physics::box_cast_2d(
            cast_world,
            {},
            {1.0f, 1.0f},
            {},
            10.0f),
        "BoxCast rejects invalid dimensions and zero direction");


    core::World rotated_query_world;

    const auto rotated_query_target =
        rotated_query_world.create(
            "Rotated Query Target");

    auto* rotated_query_box =
        rotated_query_world.add_component<
            physics::BoxCollider>(
                rotated_query_target,
                physics::box_collider_type());

    if (rotated_query_box) {
        rotated_query_box->size =
            {2.0f, 0.5f, 1.0f};
    }

    rotated_query_world.transform(
        rotated_query_target)->local_rotation =
            core::Quat{
                0.0f,
                0.0f,
                0.70710678f,
                0.70710678f
            };

    const auto rotated_ray_hit =
        physics::raycast(
            rotated_query_world,
            {-3.0f, 0.0f, 0.0f},
            {1.0f, 0.0f, 0.0f},
            10.0f);

    check(
        rotated_ray_hit &&
        rotated_ray_hit->entity ==
            rotated_query_target &&
        std::abs(
            rotated_ray_hit->distance -
            2.75f) < 0.0002f &&
        std::abs(
            rotated_ray_hit->normal.x +
            1.0f) < 0.0002f &&
        std::abs(
            rotated_ray_hit->normal.y) <
            0.0002f &&
        std::abs(
            rotated_ray_hit->normal.z) <
            0.0002f,
        "3D Raycast transforms the ray into rotated BoxCollider local axes");

    check(
        physics::overlap_box(
            rotated_query_world,
            {0.6f, 0.0f, 0.0f},
            {0.5f, 0.5f, 0.5f})
            .empty() &&
        physics::overlap_box(
            rotated_query_world,
            {0.4f, 0.0f, 0.0f},
            {0.5f, 0.5f, 0.5f})
            .size() == 1u,
        "3D OverlapBox uses OBB SAT instead of rotated target AABB approximation");

    const auto rotated_cast_hit =
        physics::box_cast(
            rotated_query_world,
            {-3.0f, 0.0f, 0.0f},
            {1.0f, 1.0f, 1.0f},
            {1.0f, 0.0f, 0.0f},
            10.0f);

    check(
        rotated_cast_hit &&
        rotated_cast_hit->entity ==
            rotated_query_target &&
        std::abs(
            rotated_cast_hit->distance -
            2.25f) < 0.0002f &&
        rotated_cast_hit->normal ==
            core::Vec3{
                -1.0f,
                0.0f,
                0.0f} &&
        std::abs(
            rotated_cast_hit->point.x +
            0.75f) < 0.0002f,
        "3D BoxCast continuous SAT returns time of impact against rotated BoxCollider");

    core::World rotated_query_2d_world;

    const auto rotated_query_2d_target =
        rotated_query_2d_world.create(
            "Rotated Query Target 2D");

    auto* rotated_query_box_2d =
        rotated_query_2d_world.add_component<
            physics::BoxCollider2D>(
                rotated_query_2d_target,
                physics::box_collider2d_type());

    if (rotated_query_box_2d) {
        rotated_query_box_2d->size =
            {2.0f, 0.5f, 0.0f};
    }

    rotated_query_2d_world.transform(
        rotated_query_2d_target)->local_rotation =
            core::Quat{
                0.0f,
                0.0f,
                0.70710678f,
                0.70710678f
            };

    const auto rotated_ray_hit_2d =
        physics::raycast_2d(
            rotated_query_2d_world,
            {-3.0f, 0.0f},
            {1.0f, 0.0f},
            10.0f);

    const auto rotated_cast_hit_2d =
        physics::box_cast_2d(
            rotated_query_2d_world,
            {-3.0f, 0.0f},
            {1.0f, 1.0f},
            {1.0f, 0.0f},
            10.0f);

    check(
        rotated_ray_hit_2d &&
        rotated_ray_hit_2d->entity ==
            rotated_query_2d_target &&
        std::abs(
            rotated_ray_hit_2d->distance -
            2.75f) < 0.0002f &&
        rotated_cast_hit_2d &&
        rotated_cast_hit_2d->entity ==
            rotated_query_2d_target &&
        std::abs(
            rotated_cast_hit_2d->distance -
            2.25f) < 0.0002f &&
        rotated_cast_hit_2d->is_2d,
        "2D Raycast and BoxCast use rotated BoxCollider2D axes");

    core::World rotated_radial_query_world;

    const auto offset_sphere =
        rotated_radial_query_world.create(
            "Rotated Offset Sphere");

    auto* offset_sphere_collider =
        rotated_radial_query_world.add_component<
            physics::SphereCollider>(
                offset_sphere,
                physics::sphere_collider_type());

    if (offset_sphere_collider) {
        offset_sphere_collider->center =
            {1.0f, 0.0f, 0.0f};
    }

    rotated_radial_query_world.transform(
        offset_sphere)->local_rotation =
            core::Quat{
                0.0f,
                0.0f,
                0.70710678f,
                0.70710678f
            };

    const auto offset_sphere_overlaps =
        physics::overlap_box(
            rotated_radial_query_world,
            {0.0f, 1.0f, 0.0f},
            {0.2f, 0.2f, 0.2f});

    const auto offset_sphere_ray =
        physics::raycast(
            rotated_radial_query_world,
            {0.0f, -2.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            10.0f);

    check(
        offset_sphere_overlaps.size() ==
            1u &&
        offset_sphere_overlaps.front() ==
            offset_sphere &&
        offset_sphere_ray &&
        offset_sphere_ray->entity ==
            offset_sphere &&
        std::abs(
            offset_sphere_ray->distance -
            2.5f) < 0.0002f,
        "rotated SphereCollider center offsets participate in physics queries");

    const auto collision_frame =
        physics::step_physics(
            collision_world,
            1.0f / 60.0f);

    check(
        collision_frame
            .collisions
            .overlaps
            .size() == 2u &&
        collision_frame
            .collisions
            .tested_pairs_3d == 1u &&
        collision_frame
            .collisions
            .tested_pairs_2d == 1u &&
        collision_frame
            .resolution
            .resolved_3d == 0u,
        "full physics frame performs integration before collision detection while leaving triggers unresolved");

    core::World resolution_world;

    const auto wall =
        resolution_world.create(
            "Wall");
    const auto dynamic_box =
        resolution_world.create(
            "Dynamic Box");

    resolution_world.add_component<
        physics::BoxCollider>(
            wall,
            physics::box_collider_type());

    resolution_world.add_component<
        physics::BoxCollider>(
            dynamic_box,
            physics::box_collider_type());

    auto* resolution_body =
        resolution_world.add_component<
            physics::Rigidbody>(
                dynamic_box,
                physics::rigidbody_type());

    resolution_world.transform(
        dynamic_box)->local_position =
            {0.75f, 0.0f, 0.0f};

    if (resolution_body) {
        resolution_body->use_gravity =
            false;
        resolution_body->linear_velocity =
            {-2.0f, 0.0f, 0.0f};
    }

    const auto resolution_detection =
        physics::detect_box_overlaps(
            resolution_world);

    const auto resolution_stats =
        physics::resolve_box_contacts_3d(
            resolution_world,
            resolution_detection.overlaps);

    resolution_body =
        resolution_world.get_component<
            physics::Rigidbody>(
                dynamic_box,
                physics::rigidbody_type());

    const auto* resolved_transform =
        resolution_world.transform(
            dynamic_box);

    check(
        resolution_stats.resolved_3d ==
            1u &&
        resolved_transform &&
        std::abs(
            resolved_transform
                ->local_position.x -
            1.0f) < 0.0001f &&
        resolution_body &&
        std::abs(
            resolution_body
                ->linear_velocity.x) <
            0.0001f,
        "3D box contact resolution separates a dynamic body from a static collider and removes entering normal velocity");

    core::World resolution_2d_world;

    const auto floor_2d =
        resolution_2d_world.create(
            "Floor2D");
    const auto dynamic_2d =
        resolution_2d_world.create(
            "Dynamic2D");

    resolution_2d_world.add_component<
        physics::BoxCollider2D>(
            floor_2d,
            physics::box_collider2d_type());

    resolution_2d_world.add_component<
        physics::BoxCollider2D>(
            dynamic_2d,
            physics::box_collider2d_type());

    auto* resolution_body_2d =
        resolution_2d_world.add_component<
            physics::Rigidbody2D>(
                dynamic_2d,
                physics::rigidbody2d_type());

    resolution_2d_world.transform(
        dynamic_2d)->local_position =
            {0.0f, 0.6f, 5.0f};

    if (resolution_body_2d) {
        resolution_body_2d->use_gravity =
            false;
        resolution_body_2d->linear_velocity =
            {0.0f, -3.0f, 7.0f};
    }

    const auto detection_2d =
        physics::detect_box_overlaps(
            resolution_2d_world);

    const auto resolution_stats_2d =
        physics::resolve_box_contacts_2d(
            resolution_2d_world,
            detection_2d.overlaps);

    resolution_body_2d =
        resolution_2d_world.get_component<
            physics::Rigidbody2D>(
                dynamic_2d,
                physics::rigidbody2d_type());

    const auto* resolved_transform_2d =
        resolution_2d_world.transform(
            dynamic_2d);

    check(
        resolution_stats_2d.resolved_2d ==
            1u &&
        resolved_transform_2d &&
        std::abs(
            resolved_transform_2d
                ->local_position.y -
            1.0f) < 0.0001f &&
        std::abs(
            resolved_transform_2d
                ->local_position.z -
            5.0f) < 0.0001f &&
        resolution_body_2d &&
        resolution_body_2d
            ->linear_velocity ==
            core::Vec3{},
        "2D box contact resolution separates only on XY and clears entering normal velocity without moving Z");




    core::World material_world;

    const auto material_floor =
        material_world.create(
            "Material Floor");

    const auto material_body_entity =
        material_world.create(
            "Material Body");

    auto* material_floor_collider =
        material_world.add_component<
            physics::BoxCollider>(
                material_floor,
                physics::box_collider_type());

    auto* material_body_collider =
        material_world.add_component<
            physics::BoxCollider>(
                material_body_entity,
                physics::box_collider_type());

    auto* material_body =
        material_world.add_component<
            physics::Rigidbody>(
                material_body_entity,
                physics::rigidbody_type());

    material_floor_collider =
        material_world.get_component<
            physics::BoxCollider>(
                material_floor,
                physics::box_collider_type());

    material_body_collider =
        material_world.get_component<
            physics::BoxCollider>(
                material_body_entity,
                physics::box_collider_type());

    if (material_floor_collider) {
        material_floor_collider->friction =
            1.0f;
        material_floor_collider->restitution =
            0.5f;
    }

    if (material_body_collider) {
        material_body_collider->friction =
            1.0f;
        material_body_collider->restitution =
            0.25f;
    }

    if (material_body) {
        material_body->use_gravity = false;
        material_body->linear_velocity =
            {2.0f, -3.0f, 0.0f};
    }

    material_world.transform(
        material_body_entity)->local_position =
            {0.0f, 0.75f, 0.0f};

    const auto material_detection =
        physics::detect_box_overlaps(
            material_world);

    const auto material_resolution =
        physics::resolve_box_contacts_3d(
            material_world,
            material_detection.overlaps);

    material_body =
        material_world.get_component<
            physics::Rigidbody>(
                material_body_entity,
                physics::rigidbody_type());

    check(
        material_resolution.resolved_3d ==
            1u &&
        material_body &&
        std::abs(
            material_body->linear_velocity.x) <
            0.0001f &&
        std::abs(
            material_body->linear_velocity.y -
            1.5f) < 0.0001f,
        "3D contact solver applies max restitution and Coulomb friction impulse");

    core::World material_2d_world;

    const auto material_floor_2d =
        material_2d_world.create(
            "Material Floor 2D");

    const auto material_body_2d_entity =
        material_2d_world.create(
            "Material Body 2D");

    auto* material_floor_collider_2d =
        material_2d_world.add_component<
            physics::BoxCollider2D>(
                material_floor_2d,
                physics::box_collider2d_type());

    auto* material_body_collider_2d =
        material_2d_world.add_component<
            physics::BoxCollider2D>(
                material_body_2d_entity,
                physics::box_collider2d_type());

    auto* material_body_2d =
        material_2d_world.add_component<
            physics::Rigidbody2D>(
                material_body_2d_entity,
                physics::rigidbody2d_type());

    material_floor_collider_2d =
        material_2d_world.get_component<
            physics::BoxCollider2D>(
                material_floor_2d,
                physics::box_collider2d_type());

    material_body_collider_2d =
        material_2d_world.get_component<
            physics::BoxCollider2D>(
                material_body_2d_entity,
                physics::box_collider2d_type());

    if (material_floor_collider_2d) {
        material_floor_collider_2d->friction =
            0.0f;
        material_floor_collider_2d->restitution =
            1.0f;
    }

    if (material_body_collider_2d) {
        material_body_collider_2d->friction =
            1.0f;
        material_body_collider_2d->restitution =
            0.0f;
    }

    if (material_body_2d) {
        material_body_2d->use_gravity = false;
        material_body_2d->linear_velocity =
            {2.0f, -3.0f, 9.0f};
    }

    material_2d_world.transform(
        material_body_2d_entity)->local_position =
            {0.0f, 0.75f, 4.0f};

    const auto material_detection_2d =
        physics::detect_box_overlaps(
            material_2d_world);

    const auto material_resolution_2d =
        physics::resolve_box_contacts_2d(
            material_2d_world,
            material_detection_2d.overlaps);

    material_body_2d =
        material_2d_world.get_component<
            physics::Rigidbody2D>(
                material_body_2d_entity,
                physics::rigidbody2d_type());

    check(
        material_resolution_2d.resolved_2d ==
            1u &&
        material_body_2d &&
        std::abs(
            material_body_2d->linear_velocity.x -
            2.0f) < 0.0001f &&
        std::abs(
            material_body_2d->linear_velocity.y -
            3.0f) < 0.0001f &&
        std::abs(
            material_body_2d->linear_velocity.z) <
            0.0001f,
        "2D contact solver combines zero friction with full restitution and keeps velocity on XY");

    core::World rotated_box_world;

    const auto rotated_box_a =
        rotated_box_world.create(
            "Rotated Box A");

    const auto rotated_box_b =
        rotated_box_world.create(
            "Rotated Box B");

    auto* rotated_collider_a =
        rotated_box_world.add_component<
            physics::BoxCollider>(
                rotated_box_a,
                physics::box_collider_type());

    if (rotated_collider_a) {
        rotated_collider_a->size =
            {2.0f, 0.2f, 0.2f};
    }

    auto* rotated_collider_b =
        rotated_box_world.add_component<
            physics::BoxCollider>(
                rotated_box_b,
                physics::box_collider_type());

    if (rotated_collider_b) {
        rotated_collider_b->size =
            {2.0f, 0.2f, 0.2f};
    }

    const core::Quat rotation_45{
        0.0f,
        0.0f,
        0.38268343f,
        0.92387953f
    };

    rotated_box_world.transform(
        rotated_box_a)->local_rotation =
            rotation_45;

    rotated_box_world.transform(
        rotated_box_b)->local_rotation =
            rotation_45;

    rotated_box_world.transform(
        rotated_box_b)->local_position =
            {-0.35355339f, 0.35355339f, 0.0f};

    const auto rotated_separated =
        physics::detect_box_overlaps(
            rotated_box_world);

    check(
        rotated_separated.tested_pairs_3d ==
            1u &&
        rotated_separated.overlaps.empty(),
        "3D rotated BoxCollider SAT rejects broad-phase AABB false positives");

    rotated_box_world.transform(
        rotated_box_b)->local_position =
            {-0.10606602f, 0.10606602f, 0.0f};

    const auto rotated_overlap =
        physics::detect_box_overlaps(
            rotated_box_world);

    check(
        rotated_overlap.overlaps.size() ==
            1u &&
        std::abs(
            rotated_overlap.overlaps.front()
                .penetration -
            0.05f) < 0.0002f &&
        std::abs(
            std::abs(
                rotated_overlap.overlaps.front()
                    .normal.x) -
            0.70710678f) < 0.0002f &&
        std::abs(
            std::abs(
                rotated_overlap.overlaps.front()
                    .normal.y) -
            0.70710678f) < 0.0002f,
        "3D rotated BoxCollider SAT returns oriented minimum penetration axis");

    core::World rotated_box_2d_world;

    const auto rotated_box_2d_a =
        rotated_box_2d_world.create(
            "Rotated Box2D A");

    const auto rotated_box_2d_b =
        rotated_box_2d_world.create(
            "Rotated Box2D B");

    auto* rotated_collider_2d_a =
        rotated_box_2d_world.add_component<
            physics::BoxCollider2D>(
                rotated_box_2d_a,
                physics::box_collider2d_type());

    if (rotated_collider_2d_a) {
        rotated_collider_2d_a->size =
            {2.0f, 0.2f, 0.0f};
    }

    auto* rotated_collider_2d_b =
        rotated_box_2d_world.add_component<
            physics::BoxCollider2D>(
                rotated_box_2d_b,
                physics::box_collider2d_type());

    if (rotated_collider_2d_b) {
        rotated_collider_2d_b->size =
            {2.0f, 0.2f, 0.0f};
    }

    rotated_box_2d_world.transform(
        rotated_box_2d_a)->local_rotation =
            rotation_45;

    rotated_box_2d_world.transform(
        rotated_box_2d_b)->local_rotation =
            rotation_45;

    rotated_box_2d_world.transform(
        rotated_box_2d_b)->local_position =
            {-0.35355339f, 0.35355339f, 8.0f};

    const auto rotated_2d_separated =
        physics::detect_box_overlaps(
            rotated_box_2d_world);

    check(
        rotated_2d_separated.tested_pairs_2d ==
            1u &&
        rotated_2d_separated.overlaps.empty(),
        "2D rotated BoxCollider2D SAT rejects XY broad-phase false positives");

    rotated_box_2d_world.transform(
        rotated_box_2d_b)->local_position =
            {-0.10606602f, 0.10606602f, 8.0f};

    const auto rotated_2d_overlap =
        physics::detect_box_overlaps(
            rotated_box_2d_world);

    check(
        rotated_2d_overlap.overlaps.size() ==
            1u &&
        rotated_2d_overlap.overlaps.front()
            .is_2d &&
        std::abs(
            rotated_2d_overlap.overlaps.front()
                .penetration -
            0.05f) < 0.0002f,
        "2D rotated BoxCollider2D SAT resolves overlap only on XY");

    core::World rotated_center_world;

    const auto centered_box =
        rotated_center_world.create(
            "Offset Rotated Box");

    const auto centered_sphere =
        rotated_center_world.create(
            "Offset Sphere");

    auto* centered_box_collider =
        rotated_center_world.add_component<
            physics::BoxCollider>(
                centered_box,
                physics::box_collider_type());

    rotated_center_world.add_component<
        physics::SphereCollider>(
            centered_sphere,
            physics::sphere_collider_type());

    if (centered_box_collider) {
        centered_box_collider->center =
            {1.0f, 0.0f, 0.0f};
    }

    rotated_center_world.transform(
        centered_box)->local_rotation =
            core::Quat{
                0.0f,
                0.0f,
                0.70710678f,
                0.70710678f
            };

    rotated_center_world.transform(
        centered_sphere)->local_position =
            {0.0f, 1.6f, 0.0f};

    const auto centered_detection =
        physics::detect_box_overlaps(
            rotated_center_world);

    check(
        centered_detection.overlaps.size() ==
            1u &&
        std::abs(
            centered_detection.overlaps.front()
                .penetration -
            0.4f) < 0.0002f &&
        centered_detection.overlaps.front()
            .normal.y > 0.99f,
        "rotated BoxCollider center offsets rotate with Transform before mixed sphere narrow phase");

    core::World sphere_world;

    const auto sphere_static =
        sphere_world.create(
            "Sphere Static");

    const auto sphere_dynamic =
        sphere_world.create(
            "Sphere Dynamic");

    sphere_world.add_component<
        physics::SphereCollider>(
            sphere_static,
            physics::sphere_collider_type());

    sphere_world.add_component<
        physics::SphereCollider>(
            sphere_dynamic,
            physics::sphere_collider_type());

    auto* sphere_body =
        sphere_world.add_component<
            physics::Rigidbody>(
                sphere_dynamic,
                physics::rigidbody_type());

    sphere_world.transform(
        sphere_dynamic)->local_position =
            {0.75f, 0.0f, 0.0f};

    if (sphere_body) {
        sphere_body->use_gravity =
            false;
        sphere_body->linear_velocity =
            {-2.0f, 0.0f, 0.0f};
    }

    const auto sphere_detection =
        physics::detect_box_overlaps(
            sphere_world);

    check(
        sphere_detection.overlaps.size() ==
            1u &&
        sphere_detection.tested_pairs_3d ==
            1u &&
        !sphere_detection.overlaps.front()
            .is_2d &&
        std::abs(
            sphere_detection.overlaps.front()
                .penetration -
            0.25f) < 0.0001f &&
        sphere_detection.overlaps.front()
            .normal ==
            core::Vec3{
                1.0f,
                0.0f,
                0.0f},
        "SphereCollider pairs participate in the shared sweep-and-prune narrow phase");

    const auto sphere_resolution =
        physics::resolve_box_contacts_3d(
            sphere_world,
            sphere_detection.overlaps);

    sphere_body =
        sphere_world.get_component<
            physics::Rigidbody>(
                sphere_dynamic,
                physics::rigidbody_type());

    const auto* sphere_transform =
        sphere_world.transform(
            sphere_dynamic);

    check(
        sphere_resolution.resolved_3d ==
            1u &&
        sphere_transform &&
        std::abs(
            sphere_transform
                ->local_position.x -
            1.0f) < 0.0001f &&
        sphere_body &&
        std::abs(
            sphere_body
                ->linear_velocity.x) <
            0.0001f,
        "SphereCollider contact resolution reuses Rigidbody separation and normal-velocity removal");

    const auto sphere_query =
        physics::overlap_box(
            sphere_world,
            {},
            {0.5f, 0.5f, 0.5f});

    const auto sphere_ray =
        physics::raycast(
            sphere_world,
            {-2.0f, 0.0f, 0.0f},
            {1.0f, 0.0f, 0.0f},
            10.0f);

    check(
        sphere_query.size() == 1u &&
        sphere_query.front() ==
            sphere_static &&
        sphere_ray &&
        sphere_ray->entity ==
            sphere_static &&
        std::abs(
            sphere_ray->distance -
            1.5f) < 0.0001f &&
        sphere_ray->normal ==
            core::Vec3{
                -1.0f,
                0.0f,
                0.0f},
        "OverlapBox and Raycast include SphereCollider candidates");

    core::World mixed_world;

    const auto mixed_box =
        mixed_world.create(
            "Mixed Box");

    const auto mixed_sphere =
        mixed_world.create(
            "Mixed Sphere");

    mixed_world.add_component<
        physics::BoxCollider>(
            mixed_box,
            physics::box_collider_type());

    mixed_world.add_component<
        physics::SphereCollider>(
            mixed_sphere,
            physics::sphere_collider_type());

    mixed_world.transform(
        mixed_sphere)->local_position =
            {0.75f, 0.0f, 0.0f};

    const auto mixed_detection =
        physics::detect_box_overlaps(
            mixed_world);

    check(
        mixed_detection.overlaps.size() ==
            1u &&
        mixed_detection.tested_pairs_3d ==
            1u &&
        std::abs(
            mixed_detection.overlaps.front()
                .penetration -
            0.25f) < 0.0001f &&
        mixed_detection.overlaps.front()
            .normal ==
            core::Vec3{
                1.0f,
                0.0f,
                0.0f},
        "BoxCollider and SphereCollider use closest-point mixed narrow phase");

    core::World circle_world;

    const auto circle_static =
        circle_world.create(
            "Circle Static");

    const auto circle_dynamic =
        circle_world.create(
            "Circle Dynamic");

    circle_world.add_component<
        physics::CircleCollider2D>(
            circle_static,
            physics::circle_collider2d_type());

    circle_world.add_component<
        physics::CircleCollider2D>(
            circle_dynamic,
            physics::circle_collider2d_type());

    circle_world.transform(
        circle_dynamic)->local_position =
            {0.0f, 0.75f, 4.0f};

    const auto circle_detection =
        physics::detect_box_overlaps(
            circle_world);

    const auto circle_query =
        physics::overlap_box_2d(
            circle_world,
            {},
            {0.5f, 0.5f});

    const auto circle_ray =
        physics::raycast_2d(
            circle_world,
            {0.0f, -2.0f},
            {0.0f, 1.0f},
            10.0f);

    check(
        circle_detection.overlaps.size() ==
            1u &&
        circle_detection.tested_pairs_2d ==
            1u &&
        circle_detection.overlaps.front()
            .is_2d &&
        std::abs(
            circle_detection.overlaps.front()
                .penetration -
            0.25f) < 0.0001f &&
        circle_detection.overlaps.front()
            .normal ==
            core::Vec3{
                0.0f,
                1.0f,
                0.0f} &&
        circle_query.size() == 1u &&
        circle_query.front() ==
            circle_static &&
        circle_ray &&
        circle_ray->entity ==
            circle_static &&
        std::abs(
            circle_ray->distance -
            1.5f) < 0.0001f &&
        circle_ray->normal ==
            core::Vec3{
                0.0f,
                -1.0f,
                0.0f},
        "CircleCollider2D participates in XY contacts OverlapBox2D and Raycast2D");

    if (failures != 0) {
        std::cerr
            << failures
            << " physics checks failed\n";
        return EXIT_FAILURE;
    }

    std::cout
        << "All physics checks passed\n";

    return EXIT_SUCCESS;
}
