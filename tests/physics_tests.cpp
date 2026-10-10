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
            physics::rigidbody2d_type()) != nullptr &&
        metadata.find(
            physics::box_collider2d_type()) != nullptr,
        "3D and 2D physics component descriptors are discoverable");

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

    check(
        body &&
        collider &&
        body2d &&
        collider2d,
        "physics components attach to World entities");

    if (body) {
        body->mass = 2.5f;
        body->gravity_scale = 0.75f;
        body->linear_velocity =
            {1.0f, 2.0f, 3.0f};
    }

    if (collider) {
        collider->is_trigger = true;
        collider->center =
            {0.25f, 0.5f, 0.75f};
        collider->size =
            {2.0f, 3.0f, 4.0f};
    }

    if (body2d) {
        body2d->use_gravity = false;
        body2d->mass = 3.0f;
        body2d->linear_velocity =
            {4.0f, 5.0f, 0.0f};
    }

    if (collider2d) {
        collider2d->size =
            {6.0f, 7.0f, 0.0f};
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

    check(
        captured_body &&
        captured_collider &&
        captured_body2d &&
        captured_collider2d,
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

    check(
        restored_body &&
        std::abs(
            restored_body->mass -
            2.5f) < 0.0001f &&
        restored_body->linear_velocity ==
            core::Vec3{1.0f, 2.0f, 3.0f} &&
        restored_collider &&
        restored_collider->is_trigger &&
        restored_collider->size ==
            core::Vec3{2.0f, 3.0f, 4.0f} &&
        restored_body2d &&
        !restored_body2d->use_gravity &&
        restored_body2d->linear_velocity ==
            core::Vec3{4.0f, 5.0f, 0.0f} &&
        restored_collider2d &&
        restored_collider2d->size ==
            core::Vec3{6.0f, 7.0f, 0.0f},
        "physics Scene roundtrip preserves configured values");

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
            3u &&
        detection.tested_pairs_2d ==
            1u &&
        detection.overlaps.size() ==
            2u,
        "box overlap detection separates 3D and 2D pair scans");

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
            .tested_pairs_3d == 3u &&
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
