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
    bool_property(
        "Allow Sleep",
        &Component::allow_sleep);

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

    positive_float_property(
        "Sleep Threshold",
        &Component::sleep_threshold,
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
                        component.sleeping = false;
                        component.sleep_timer = 0.0f;
                        return true;
                    });
            }) &&
        ok;

    ok =
        properties.register_property(
            type,
            "Sleeping",
            core::PropertyKind::Boolean,
            [type](
                const core::World& world,
                core::Entity entity) {
                return read_component_property<Component>(
                    world,
                    entity,
                    type,
                    [](const Component& value) {
                        return core::PropertyValue{
                            value.sleeping};
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
                            std::get_if<bool>(&raw);

                        if (!typed) return false;

                        component.sleeping =
                            *typed &&
                            component.allow_sleep;

                        component.sleep_timer =
                            0.0f;

                        if (component.sleeping) {
                            component.linear_velocity =
                                {};
                        }

                        return true;
                    });
            }) &&
        ok;

    return ok;
}

template <typename Component>
bool register_contact_material_properties(
    PropertyAccessRegistry& properties,
    core::ComponentTypeId type) {

    bool ok = true;

    const auto bounded_float =
        [&properties, type, &ok](
            const char* name,
            float Component::* member) {

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
                                    std::get_if<double>(&raw);

                                if (!typed ||
                                    *typed < 0.0 ||
                                    *typed > 1.0) {
                                    return false;
                                }

                                component.*member =
                                    static_cast<float>(
                                        *typed);
                                return true;
                            });
                    }) &&
                ok;
        };

    bounded_float(
        "Friction",
        &Component::friction);

    bounded_float(
        "Restitution",
        &Component::restitution);

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

    const auto integer_property =
        [&properties, type, &ok](
            const char* name,
            std::uint32_t Component::* member,
            std::int64_t minimum,
            std::int64_t maximum) {

            ok =
                properties.register_property(
                    type,
                    name,
                    core::PropertyKind::Integer,
                    [type, member](
                        const core::World& world,
                        core::Entity entity) {
                        return read_component_property<Component>(
                            world,
                            entity,
                            type,
                            [member](const Component& value) {
                                return core::PropertyValue{
                                    static_cast<std::int64_t>(
                                        value.*member)};
                            });
                    },
                    [type, member, minimum, maximum](
                        core::World& world,
                        core::Entity entity,
                        const core::PropertyValue& value) {
                        return write_component_property<Component>(
                            world,
                            entity,
                            type,
                            value,
                            [member, minimum, maximum](
                                Component& component,
                                const core::PropertyValue& raw) {
                                const auto* typed =
                                    std::get_if<std::int64_t>(&raw);

                                if (!typed ||
                                    *typed < minimum ||
                                    *typed > maximum) {
                                    return false;
                                }

                                component.*member =
                                    static_cast<std::uint32_t>(
                                        *typed);
                                return true;
                            });
                    }) &&
                ok;
        };

    integer_property(
        "Layer",
        &Component::layer,
        0,
        31);

    integer_property(
        "Collision Mask",
        &Component::collision_mask,
        0,
        static_cast<std::int64_t>(
            0xffffffffu));

    ok =
        register_contact_material_properties<Component>(
            properties,
            type) &&
        ok;

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

template <typename Component>
bool register_radial_properties(
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
        "Is Trigger",
        &Component::is_trigger);

    const auto integer_property =
        [&properties, type, &ok](
            const char* name,
            std::uint32_t Component::* member,
            std::int64_t minimum,
            std::int64_t maximum) {

            ok =
                properties.register_property(
                    type,
                    name,
                    core::PropertyKind::Integer,
                    [type, member](
                        const core::World& world,
                        core::Entity entity) {
                        return read_component_property<Component>(
                            world,
                            entity,
                            type,
                            [member](const Component& value) {
                                return core::PropertyValue{
                                    static_cast<std::int64_t>(
                                        value.*member)};
                            });
                    },
                    [type, member, minimum, maximum](
                        core::World& world,
                        core::Entity entity,
                        const core::PropertyValue& value) {
                        return write_component_property<Component>(
                            world,
                            entity,
                            type,
                            value,
                            [member, minimum, maximum](
                                Component& component,
                                const core::PropertyValue& raw) {
                                const auto* typed =
                                    std::get_if<std::int64_t>(&raw);

                                if (!typed ||
                                    *typed < minimum ||
                                    *typed > maximum) {
                                    return false;
                                }

                                component.*member =
                                    static_cast<std::uint32_t>(
                                        *typed);
                                return true;
                            });
                    }) &&
                ok;
        };

    integer_property(
        "Layer",
        &Component::layer,
        0,
        31);

    integer_property(
        "Collision Mask",
        &Component::collision_mask,
        0,
        static_cast<std::int64_t>(
            0xffffffffu));

    ok =
        register_contact_material_properties<Component>(
            properties,
            type) &&
        ok;

    ok =
        properties.register_property(
            type,
            "Center",
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
                            value.center};
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
                        component.center = *typed;
                        return true;
                    });
            }) &&
        ok;

    ok =
        properties.register_property(
            type,
            "Radius",
            core::PropertyKind::Float,
            [type](
                const core::World& world,
                core::Entity entity) {
                return read_component_property<Component>(
                    world,
                    entity,
                    type,
                    [](const Component& value) {
                        return core::PropertyValue{
                            static_cast<double>(
                                value.radius)};
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
                            std::get_if<double>(&raw);
                        if (!typed ||
                            *typed <= 0.0) {
                            return false;
                        }

                        component.radius =
                            static_cast<float>(
                                *typed);
                        return true;
                    });
            }) &&
        ok;

    return ok;
}

template <typename Component>
bool register_capsule_common_properties(
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

    bool_property("Enabled", &Component::enabled);
    bool_property("Is Trigger", &Component::is_trigger);

    const auto integer_property =
        [&properties, type, &ok](
            const char* name,
            std::uint32_t Component::* member,
            std::int64_t minimum,
            std::int64_t maximum) {

            ok =
                properties.register_property(
                    type,
                    name,
                    core::PropertyKind::Integer,
                    [type, member](
                        const core::World& world,
                        core::Entity entity) {
                        return read_component_property<Component>(
                            world,
                            entity,
                            type,
                            [member](const Component& value) {
                                return core::PropertyValue{
                                    static_cast<std::int64_t>(
                                        value.*member)};
                            });
                    },
                    [type, member, minimum, maximum](
                        core::World& world,
                        core::Entity entity,
                        const core::PropertyValue& value) {
                        return write_component_property<Component>(
                            world,
                            entity,
                            type,
                            value,
                            [member, minimum, maximum](
                                Component& component,
                                const core::PropertyValue& raw) {
                                const auto* typed =
                                    std::get_if<std::int64_t>(&raw);

                                if (!typed ||
                                    *typed < minimum ||
                                    *typed > maximum) {
                                    return false;
                                }

                                component.*member =
                                    static_cast<std::uint32_t>(
                                        *typed);
                                return true;
                            });
                    }) &&
                ok;
        };

    integer_property(
        "Layer",
        &Component::layer,
        0,
        31);
    integer_property(
        "Collision Mask",
        &Component::collision_mask,
        0,
        static_cast<std::int64_t>(
            0xffffffffu));

    ok =
        register_contact_material_properties<Component>(
            properties,
            type) &&
        ok;

    ok =
        properties.register_property(
            type,
            "Center",
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
                            value.center};
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
                        component.center = *typed;
                        return true;
                    });
            }) &&
        ok;

    return ok;
}

bool register_capsule_properties(
    PropertyAccessRegistry& properties) {

    const auto type =
        physics::capsule_collider_type();

    bool ok =
        register_capsule_common_properties<
            physics::CapsuleCollider>(
                properties,
                type);

    const auto float_property =
        [&properties, type, &ok](
            const char* name,
            float physics::CapsuleCollider::* member) {

            ok =
                properties.register_property(
                    type,
                    name,
                    core::PropertyKind::Float,
                    [type, member](
                        const core::World& world,
                        core::Entity entity) {
                        return read_component_property<
                            physics::CapsuleCollider>(
                                world,
                                entity,
                                type,
                                [member](
                                    const physics::CapsuleCollider& value) {
                                    return core::PropertyValue{
                                        static_cast<double>(
                                            value.*member)};
                                });
                    },
                    [type, member](
                        core::World& world,
                        core::Entity entity,
                        const core::PropertyValue& value) {
                        return write_component_property<
                            physics::CapsuleCollider>(
                                world,
                                entity,
                                type,
                                value,
                                [member](
                                    physics::CapsuleCollider& component,
                                    const core::PropertyValue& raw) {
                                    const auto* typed =
                                        std::get_if<double>(&raw);
                                    if (!typed ||
                                        *typed <= 0.0) {
                                        return false;
                                    }
                                    component.*member =
                                        static_cast<float>(*typed);
                                    return component.height >=
                                        component.radius * 2.0f;
                                });
                    }) &&
                ok;
        };

    float_property(
        "Radius",
        &physics::CapsuleCollider::radius);
    float_property(
        "Height",
        &physics::CapsuleCollider::height);

    ok =
        properties.register_property(
            type,
            "Direction",
            core::PropertyKind::Integer,
            [type](
                const core::World& world,
                core::Entity entity) {
                return read_component_property<
                    physics::CapsuleCollider>(
                        world,
                        entity,
                        type,
                        [](const physics::CapsuleCollider& value) {
                            return core::PropertyValue{
                                static_cast<std::int64_t>(
                                    value.direction)};
                        });
            },
            [type](
                core::World& world,
                core::Entity entity,
                const core::PropertyValue& value) {
                return write_component_property<
                    physics::CapsuleCollider>(
                        world,
                        entity,
                        type,
                        value,
                        [](physics::CapsuleCollider& component,
                           const core::PropertyValue& raw) {
                            const auto* typed =
                                std::get_if<std::int64_t>(&raw);
                            if (!typed ||
                                *typed < 0 ||
                                *typed > 2) {
                                return false;
                            }
                            component.direction =
                                static_cast<std::uint32_t>(*typed);
                            return true;
                        });
            }) &&
        ok;

    return ok;
}

bool register_capsule2d_properties(
    PropertyAccessRegistry& properties) {

    const auto type =
        physics::capsule_collider2d_type();

    bool ok =
        register_capsule_common_properties<
            physics::CapsuleCollider2D>(
                properties,
                type);

    ok =
        properties.register_property(
            type,
            "Size",
            core::PropertyKind::Vec3,
            [type](
                const core::World& world,
                core::Entity entity) {
                return read_component_property<
                    physics::CapsuleCollider2D>(
                        world,
                        entity,
                        type,
                        [](const physics::CapsuleCollider2D& value) {
                            return core::PropertyValue{
                                value.size};
                        });
            },
            [type](
                core::World& world,
                core::Entity entity,
                const core::PropertyValue& value) {
                return write_component_property<
                    physics::CapsuleCollider2D>(
                        world,
                        entity,
                        type,
                        value,
                        [](physics::CapsuleCollider2D& component,
                           const core::PropertyValue& raw) {
                            const auto* typed =
                                std::get_if<core::Vec3>(&raw);
                            if (!typed ||
                                typed->x <= 0.0f ||
                                typed->y <= 0.0f) {
                                return false;
                            }
                            component.size = *typed;
                            component.size.z = 0.0f;
                            return true;
                        });
            }) &&
        ok;

    ok =
        properties.register_property(
            type,
            "Direction",
            core::PropertyKind::Integer,
            [type](
                const core::World& world,
                core::Entity entity) {
                return read_component_property<
                    physics::CapsuleCollider2D>(
                        world,
                        entity,
                        type,
                        [](const physics::CapsuleCollider2D& value) {
                            return core::PropertyValue{
                                static_cast<std::int64_t>(
                                    value.direction)};
                        });
            },
            [type](
                core::World& world,
                core::Entity entity,
                const core::PropertyValue& value) {
                return write_component_property<
                    physics::CapsuleCollider2D>(
                        world,
                        entity,
                        type,
                        value,
                        [](physics::CapsuleCollider2D& component,
                           const core::PropertyValue& raw) {
                            const auto* typed =
                                std::get_if<std::int64_t>(&raw);
                            if (!typed ||
                                *typed < 0 ||
                                *typed > 1) {
                                return false;
                            }
                            component.direction =
                                static_cast<std::uint32_t>(*typed);
                            return true;
                        });
            }) &&
        ok;

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
        register_radial_properties<
            physics::SphereCollider>(
                properties,
                physics::sphere_collider_type()) &&
        ok;

    ok =
        register_capsule_properties(
            properties) &&
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

    ok =
        register_radial_properties<
            physics::CircleCollider2D>(
                properties,
                physics::circle_collider2d_type()) &&
        ok;

    ok =
        register_capsule2d_properties(
            properties) &&
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
            physics::sphere_collider_type(),
            [](core::World& world,
               core::Entity entity) {
                return world.add_component<
                    physics::SphereCollider>(
                        entity,
                        physics::sphere_collider_type()) != nullptr;
            }) &&
        ok;

    ok =
        factories.register_factory(
            physics::capsule_collider_type(),
            [](core::World& world,
               core::Entity entity) {
                return world.add_component<
                    physics::CapsuleCollider>(
                        entity,
                        physics::capsule_collider_type()) != nullptr;
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

    ok =
        factories.register_factory(
            physics::circle_collider2d_type(),
            [](core::World& world,
               core::Entity entity) {
                return world.add_component<
                    physics::CircleCollider2D>(
                        entity,
                        physics::circle_collider2d_type()) != nullptr;
            }) &&
        ok;

    ok =
        factories.register_factory(
            physics::capsule_collider2d_type(),
            [](core::World& world,
               core::Entity entity) {
                return world.add_component<
                    physics::CapsuleCollider2D>(
                        entity,
                        physics::capsule_collider2d_type()) != nullptr;
            }) &&
        ok;

    return ok;
}

} // namespace nengine::editor
