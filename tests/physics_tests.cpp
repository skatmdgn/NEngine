#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

#include "nengine/core/component_registry.hpp"
#include "nengine/core/component_serialization.hpp"
#include "nengine/core/world.hpp"
#include "nengine/physics/components.hpp"
#include "nengine/physics/registration.hpp"

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
