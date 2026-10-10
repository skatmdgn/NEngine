#include "nengine/editor/physics_integration.hpp"

#include <optional>
#include <variant>

#include "nengine/physics/components.hpp"
#include "nengine/physics/registration.hpp"

namespace nengine::editor {
namespace {

template <typename Component, typename Getter>
std::optional<core::PropertyValue> read_component_property(
    const core::World& world,
    core::Entity entity,
    core::ComponentTypeId type,
    Getter getter) {

    const auto* component =
        world.get_component<Component>(
            entity,
            type);

    if (!component) return std::nullopt;
    return getter(*component);
}

template <typename Component, typename Setter>
bool write_component_property(
    core::World& world,
    core::Entity entity,
    core::ComponentTypeId type,
    const core::PropertyValue& value,
    Setter setter) {

    auto* component =
        world.get_component<Component>(
            entity,
            type);

    return component &&
        setter(*component, value);
}

template <typename Component>
bool register_rigidbody_properties(
    PropertyAccessRegistry& properties,
    core::ComponentTypeId type) {

    bool ok = true;

    const auto bool_property =
        [&properties, type, &ok](
            const char* name,
            bool Component::* member) {

            ok =
                properties.register_property(
                    type,
                    name,
                    core::PropertyKind::Boolean,
                    [type, member](
                        const core::World& world,
                        core::Entity entity) {
                        return read_component_property<Component>(
                            world,
                            entity,
                            type,
                            [member](const Component& value) {
                                return core::PropertyValue{
                                    value.*member};
                            });
                    },
                    [type, member](
                        core::World& world,
                        core::Entity entity,
                        const core::PropertyValue& value) {
                        return write_component_property<Component>(
                            world,
                            entity,
                            type,
                            value,
                            [member](
                                Component& component,
                                const core::PropertyValue& raw) {
                                const auto* typed =
                                    std::get_if<bool>(&raw);
                                if (!typed) return false;
                                component.*member = *typed;
                                return true;
                            });
                    }) &&
                ok;
        };

    bool_property(
        "Enabled",
        &Component::enabled);
    bool_property(
        "Use Gravity",
        &Component::use_gravity);
    bool_property(
        "Is Kinematic",
        &Component::is_kinematic);

    const auto positive_float_property =
        [&properties, type, &ok](
            const char* name,
            float Component::* member,
            bool allow_zero) {

            ok =
                properties.register_property(
                    type,
                    name,
                    core::PropertyKind::Float,
                    [type, member](
                        const core::World& world,
                        core::Entity entity) {
                        return read_component_property<Component>(
                            world,
                            entity,
                            type,
                            [member](const Component& value) {
                                return core::PropertyValue{
                                    static_cast<double>(
                                        value.*member)};
                            });
                    },
                    [type, member, allow_zero](
                        core::World& world,
                        core::Entity entity,
                        const core::PropertyValue& value) {
                        return write_component_property<Component>(
                            world,
                            entity,
                            type,
                            value,
                            [member, allow_zero](
                                Component& component,
                                const core::PropertyValue& raw) {
                                const auto* typed =
                                    std::get_if<double>(&raw);
                                if (!typed ||
                                    (allow_zero
                                        ? *typed < 0.0
                                        : *typed <= 0.0)) {
                                    return false;
                                }
                                component.*member =
                                    static_cast<float>(*typed);
                                return true;
                            });
                    }) &&
                ok;
        };

    positive_float_property(
        "Mass",
        &Component::mass,
        false);

    positive_float_property(
        "Gravity Scale",
        &Component::gravity_scale,
        true);

    ok =
        properties.register_property(
            type,
            "Linear Velocity",
            core::PropertyKind::Vec3,
            [type](
                const core::World& world,
                core::Entity entity) {
                return read_component_property<Component>(
                    world,
                    entity,
                    type,
                    [](const Component& value) {
                        return core::PropertyValue{
                            value.linear_velocity};
                    });
            },
            [type](
                core::World& world,
                core::Entity entity,
                const core::PropertyValue& value) {
                return write_component_property<Component>(
                    world,
                    entity,
                    type,
                    value,
                    [](Component& component,
                       const core::PropertyValue& raw) {
                        const auto* typed =
                            std::get_if<core::Vec3>(&raw);
                        if (!typed) return false;
                        component.linear_velocity = *typed;
                        return true;
                    });
            }) &&
        ok;

    return ok;
}

template <typename Component>
bool register_box_properties(
    PropertyAccessRegistry& properties,
    core::ComponentTypeId type,
    bool is_2d) {

    bool ok = true;

    const auto bool_property =
        [&properties, type, &ok](
            const char* name,
            bool Component::* member) {

            ok =
                properties.register_property(
                    type,
                    name,
                    core::PropertyKind::Boolean,
                    [type, member](
                        const core::World& world,
                        core::Entity entity) {
                        return read_component_property<Component>(
                            world,
                            entity,
                            type,
                            [member](const Component& value) {
                                return core::PropertyValue{
                                    value.*member};
                            });
                    },
                    [type, member](
                        core::World& world,
                        core::Entity entity,
                        const core::PropertyValue& value) {
                        return write_component_property<Component>(
                            world,
                            entity,
                            type,
                            value,
                            [member](
                                Component& component,
                                const core::PropertyValue& raw) {
                                const auto* typed =
                                    std::get_if<bool>(&raw);
                                if (!typed) return false;
                                component.*member = *typed;
                                return true;
                            });
                    }) &&
                ok;
        };

    bool_property(
        "Enabled",
        &Component::enabled);
    bool_property(
        "Is Trigger",
        &Component::is_trigger);

    const auto vec_property =
        [&properties, type, is_2d, &ok](
            const char* name,
            core::Vec3 Component::* member,
            bool validate_size) {

            ok =
                properties.register_property(
                    type,
                    name,
                    core::PropertyKind::Vec3,
                    [type, member](
                        const core::World& world,
                        core::Entity entity) {
                        return read_component_property<Component>(
                            world,
                            entity,
                            type,
                            [member](const Component& value) {
                                return core::PropertyValue{
                                    value.*member};
                            });
                    },
                    [type, member, validate_size, is_2d](
                        core::World& world,
                        core::Entity entity,
                        const core::PropertyValue& value) {
                        return write_component_property<Component>(
                            world,
                            entity,
                            type,
                            value,
                            [member, validate_size, is_2d](
                                Component& component,
                                const core::PropertyValue& raw) {
                                const auto* typed =
                                    std::get_if<core::Vec3>(&raw);
                                if (!typed) return false;

                                if (validate_size &&
                                    (typed->x <= 0.0f ||
                                     typed->y <= 0.0f ||
                                     (!is_2d &&
                                      typed->z <= 0.0f))) {
                                    return false;
                                }

                                component.*member = *typed;
                                return true;
                            });
                    }) &&
                ok;
        };

    vec_property(
        "Center",
        &Component::center,
        false);

    vec_property(
        "Size",
        &Component::size,
        true);

    return ok;
}

} // namespace

bool register_physics_integration(
    core::ComponentRegistry& components,
    core::ComponentSerializationRegistry& serialization,
    PropertyAccessRegistry& properties) {

    bool ok = true;

    ok =
        physics::register_component_metadata(
            components) &&
        ok;

    ok =
        physics::register_component_serializers(
            serialization) &&
        ok;

    ok =
        register_rigidbody_properties<
            physics::Rigidbody>(
                properties,
                physics::rigidbody_type()) &&
        ok;

    ok =
        register_box_properties<
            physics::BoxCollider>(
                properties,
                physics::box_collider_type(),
                false) &&
        ok;

    ok =
        register_rigidbody_properties<
            physics::Rigidbody2D>(
                properties,
                physics::rigidbody2d_type()) &&
        ok;

    ok =
        register_box_properties<
            physics::BoxCollider2D>(
                properties,
                physics::box_collider2d_type(),
                true) &&
        ok;

    return ok;
}

bool register_physics_component_factories(
    ComponentFactoryRegistry& factories) {

    bool ok = true;

    ok =
        factories.register_factory(
            physics::rigidbody_type(),
            [](core::World& world,
               core::Entity entity) {
                return world.add_component<
                    physics::Rigidbody>(
                        entity,
                        physics::rigidbody_type()) != nullptr;
            }) &&
        ok;

    ok =
        factories.register_factory(
            physics::box_collider_type(),
            [](core::World& world,
               core::Entity entity) {
                return world.add_component<
                    physics::BoxCollider>(
                        entity,
                        physics::box_collider_type()) != nullptr;
            }) &&
        ok;

    ok =
        factories.register_factory(
            physics::rigidbody2d_type(),
            [](core::World& world,
               core::Entity entity) {
                return world.add_component<
                    physics::Rigidbody2D>(
                        entity,
                        physics::rigidbody2d_type()) != nullptr;
            }) &&
        ok;

    ok =
        factories.register_factory(
            physics::box_collider2d_type(),
            [](core::World& world,
               core::Entity entity) {
                return world.add_component<
                    physics::BoxCollider2D>(
                        entity,
                        physics::box_collider2d_type()) != nullptr;
            }) &&
        ok;

    return ok;
}

} // namespace nengine::editor
