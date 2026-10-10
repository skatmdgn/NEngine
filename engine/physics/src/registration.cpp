#include "nengine/physics/registration.hpp"

#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "nengine/physics/components.hpp"

namespace nengine::physics {
namespace {

const core::SerializedPropertyData* find_property(
    const core::SerializedComponentData& data,
    std::string_view name) {

    for (const auto& property : data.properties) {
        if (property.name == name) {
            return &property;
        }
    }

    return nullptr;
}

bool read_bool(
    const core::SerializedComponentData& data,
    std::string_view name,
    bool& value,
    bool optional = false) {

    const auto* property =
        find_property(data, name);

    if (!property) return optional;

    const auto* typed =
        std::get_if<bool>(&property->value);

    if (!typed) return false;

    value = *typed;
    return true;
}

bool read_float(
    const core::SerializedComponentData& data,
    std::string_view name,
    float& value,
    bool optional = false) {

    const auto* property =
        find_property(data, name);

    if (!property) return optional;

    const auto* typed =
        std::get_if<double>(&property->value);

    if (!typed) return false;

    value = static_cast<float>(*typed);
    return true;
}

bool read_integer(
    const core::SerializedComponentData& data,
    std::string_view name,
    std::int64_t& value,
    bool optional = false) {

    const auto* property =
        find_property(data, name);

    if (!property) return optional;

    const auto* typed =
        std::get_if<std::int64_t>(
            &property->value);

    if (!typed) return false;

    value = *typed;
    return true;
}

bool read_vec3(
    const core::SerializedComponentData& data,
    std::string_view name,
    core::Vec3& value) {

    const auto* property =
        find_property(data, name);

    if (!property) return false;

    const auto* typed =
        std::get_if<core::Vec3>(
            &property->value);

    if (!typed) return false;

    value = *typed;
    return true;
}

core::SerializedPropertyData bool_property(
    std::string name,
    bool value) {

    return {
        std::move(name),
        core::PropertyKind::Boolean,
        core::PropertyValue{value}
    };
}

core::SerializedPropertyData float_property(
    std::string name,
    float value) {

    return {
        std::move(name),
        core::PropertyKind::Float,
        core::PropertyValue{
            static_cast<double>(value)}
    };
}

core::SerializedPropertyData integer_property(
    std::string name,
    std::int64_t value) {

    return {
        std::move(name),
        core::PropertyKind::Integer,
        core::PropertyValue{value}
    };
}

core::SerializedPropertyData vec3_property(
    std::string name,
    core::Vec3 value) {

    return {
        std::move(name),
        core::PropertyKind::Vec3,
        core::PropertyValue{value}
    };
}

bool valid_rigidbody(
    float mass,
    float gravity_scale,
    float sleep_threshold) noexcept {

    return mass > 0.0f &&
        gravity_scale >= 0.0f &&
        sleep_threshold >= 0.0f;
}

bool valid_box_size(
    core::Vec3 size,
    bool is_2d) noexcept {

    return size.x > 0.0f &&
           size.y > 0.0f &&
           (is_2d || size.z > 0.0f);
}

bool valid_radius(
    float radius) noexcept {

    return radius > 0.0f;
}

bool valid_contact_material(
    float friction,
    float restitution) noexcept {

    return friction >= 0.0f &&
        friction <= 1.0f &&
        restitution >= 0.0f &&
        restitution <= 1.0f;
}

bool valid_capsule(
    float radius,
    float height,
    std::uint32_t direction) noexcept {

    return radius > 0.0f &&
        height >= radius * 2.0f &&
        direction <= 2u;
}

bool valid_capsule2d(
    core::Vec3 size,
    std::uint32_t direction) noexcept {

    return size.x > 0.0f &&
        size.y > 0.0f &&
        direction <= 1u;
}

template <typename T>
T* ensure_component(
    core::World& world,
    core::Entity entity,
    core::ComponentTypeId type) {

    auto* component =
        world.get_component<T>(
            entity,
            type);

    if (!component) {
        component =
            world.add_component<T>(
                entity,
                type);
    }

    return component;
}

bool register_rigidbody_metadata(
    core::ComponentRegistry& registry,
    std::string type_name,
    core::ComponentTypeId type,
    std::string category) {

    bool ok =
        registry.register_type(
            std::move(type_name),
            std::move(category),
            true,
            false);

    const auto flags =
        core::PropertyFlags::Serializable |
        core::PropertyFlags::Editable;

    for (const auto& [name, kind] :
         std::initializer_list<
             std::pair<
                 const char*,
                 core::PropertyKind>>{
            {"Enabled", core::PropertyKind::Boolean},
            {"Use Gravity", core::PropertyKind::Boolean},
            {"Is Kinematic", core::PropertyKind::Boolean},
            {"Allow Sleep", core::PropertyKind::Boolean},
            {"Mass", core::PropertyKind::Float},
            {"Gravity Scale", core::PropertyKind::Float},
            {"Sleep Threshold", core::PropertyKind::Float},
            {"Linear Velocity", core::PropertyKind::Vec3}}) {

        ok =
            registry.register_property(
                type,
                {
                    name,
                    kind,
                    flags
                }) &&
            ok;
    }

    return ok;
}

bool register_box_metadata(
    core::ComponentRegistry& registry,
    std::string type_name,
    core::ComponentTypeId type,
    std::string category) {

    bool ok =
        registry.register_type(
            std::move(type_name),
            std::move(category),
            true,
            false);

    const auto flags =
        core::PropertyFlags::Serializable |
        core::PropertyFlags::Editable;

    for (const auto& [name, kind] :
         std::initializer_list<
             std::pair<
                 const char*,
                 core::PropertyKind>>{
            {"Enabled", core::PropertyKind::Boolean},
            {"Is Trigger", core::PropertyKind::Boolean},
            {"Layer", core::PropertyKind::Integer},
            {"Collision Mask", core::PropertyKind::Integer},
            {"Friction", core::PropertyKind::Float},
            {"Restitution", core::PropertyKind::Float},
            {"Center", core::PropertyKind::Vec3},
            {"Size", core::PropertyKind::Vec3}}) {

        ok =
            registry.register_property(
                type,
                {
                    name,
                    kind,
                    flags
                }) &&
            ok;
    }

    return ok;
}

bool register_radial_metadata(
    core::ComponentRegistry& registry,
    std::string type_name,
    core::ComponentTypeId type,
    std::string category) {

    bool ok =
        registry.register_type(
            std::move(type_name),
            std::move(category),
            true,
            false);

    const auto flags =
        core::PropertyFlags::Serializable |
        core::PropertyFlags::Editable;

    for (const auto& [name, kind] :
         std::initializer_list<
             std::pair<
                 const char*,
                 core::PropertyKind>>{
            {"Enabled", core::PropertyKind::Boolean},
            {"Is Trigger", core::PropertyKind::Boolean},
            {"Layer", core::PropertyKind::Integer},
            {"Collision Mask", core::PropertyKind::Integer},
            {"Friction", core::PropertyKind::Float},
            {"Restitution", core::PropertyKind::Float},
            {"Center", core::PropertyKind::Vec3},
            {"Radius", core::PropertyKind::Float}}) {

        ok =
            registry.register_property(
                type,
                {
                    name,
                    kind,
                    flags
                }) &&
            ok;
    }

    return ok;
}

bool register_capsule_metadata(
    core::ComponentRegistry& registry,
    std::string type_name,
    core::ComponentTypeId type,
    std::string category,
    bool is_2d) {

    bool ok =
        registry.register_type(
            std::move(type_name),
            std::move(category),
            true,
            false);

    const auto flags =
        core::PropertyFlags::Serializable |
        core::PropertyFlags::Editable;

    const auto common =
        std::initializer_list<
            std::pair<
                const char*,
                core::PropertyKind>>{
            {"Enabled", core::PropertyKind::Boolean},
            {"Is Trigger", core::PropertyKind::Boolean},
            {"Layer", core::PropertyKind::Integer},
            {"Collision Mask", core::PropertyKind::Integer},
            {"Friction", core::PropertyKind::Float},
            {"Restitution", core::PropertyKind::Float},
            {"Center", core::PropertyKind::Vec3}
        };

    for (const auto& [name, kind] : common) {
        ok =
            registry.register_property(
                type,
                {name, kind, flags}) &&
            ok;
    }

    if (is_2d) {
        ok =
            registry.register_property(
                type,
                {"Size",
                 core::PropertyKind::Vec3,
                 flags}) &&
            ok;
    } else {
        ok =
            registry.register_property(
                type,
                {"Radius",
                 core::PropertyKind::Float,
                 flags}) &&
            ok;

        ok =
            registry.register_property(
                type,
                {"Height",
                 core::PropertyKind::Float,
                 flags}) &&
            ok;
    }

    ok =
        registry.register_property(
            type,
            {"Direction",
             core::PropertyKind::Integer,
             flags}) &&
        ok;

    return ok;
}

template <typename T>
std::optional<core::SerializedComponentData>
capture_rigidbody(
    const core::World& world,
    core::Entity entity,
    core::ComponentTypeId type) {

    const auto* value =
        world.get_component<T>(
            entity,
            type);

    if (!value) {
        return std::nullopt;
    }

    core::SerializedComponentData data;
    data.properties = {
        bool_property(
            "Enabled",
            value->enabled),
        bool_property(
            "Use Gravity",
            value->use_gravity),
        bool_property(
            "Is Kinematic",
            value->is_kinematic),
        bool_property(
            "Allow Sleep",
            value->allow_sleep),
        float_property(
            "Mass",
            value->mass),
        float_property(
            "Gravity Scale",
            value->gravity_scale),
        float_property(
            "Sleep Threshold",
            value->sleep_threshold),
        vec3_property(
            "Linear Velocity",
            value->linear_velocity)
    };

    return data;
}

template <typename T>
bool restore_rigidbody(
    core::World& world,
    core::Entity entity,
    core::ComponentTypeId type,
    const core::SerializedComponentData& data,
    std::string_view type_name,
    std::string* error) {

    T value;

    if (!read_bool(
            data,
            "Enabled",
            value.enabled) ||
        !read_bool(
            data,
            "Use Gravity",
            value.use_gravity) ||
        !read_bool(
            data,
            "Is Kinematic",
            value.is_kinematic) ||
        !read_bool(
            data,
            "Allow Sleep",
            value.allow_sleep,
            true) ||
        !read_float(
            data,
            "Mass",
            value.mass) ||
        !read_float(
            data,
            "Gravity Scale",
            value.gravity_scale) ||
        !read_float(
            data,
            "Sleep Threshold",
            value.sleep_threshold,
            true) ||
        !read_vec3(
            data,
            "Linear Velocity",
            value.linear_velocity) ||
        !valid_rigidbody(
            value.mass,
            value.gravity_scale,
            value.sleep_threshold)) {

        if (error) {
            *error =
                "malformed " +
                std::string(type_name) +
                " data";
        }
        return false;
    }

    auto* component =
        ensure_component<T>(
            world,
            entity,
            type);

    if (!component) return false;

    *component = value;
    return true;
}

template <typename T>
std::optional<core::SerializedComponentData>
capture_box(
    const core::World& world,
    core::Entity entity,
    core::ComponentTypeId type) {

    const auto* value =
        world.get_component<T>(
            entity,
            type);

    if (!value) {
        return std::nullopt;
    }

    core::SerializedComponentData data;
    data.properties = {
        bool_property(
            "Enabled",
            value->enabled),
        bool_property(
            "Is Trigger",
            value->is_trigger),
        integer_property(
            "Layer",
            static_cast<std::int64_t>(
                value->layer)),
        integer_property(
            "Collision Mask",
            static_cast<std::int64_t>(
                value->collision_mask)),
        float_property(
            "Friction",
            value->friction),
        float_property(
            "Restitution",
            value->restitution),
        vec3_property(
            "Center",
            value->center),
        vec3_property(
            "Size",
            value->size)
    };

    return data;
}

template <typename T>
bool restore_box(
    core::World& world,
    core::Entity entity,
    core::ComponentTypeId type,
    const core::SerializedComponentData& data,
    bool is_2d,
    std::string_view type_name,
    std::string* error) {

    T value;

    std::int64_t layer =
        static_cast<std::int64_t>(
            value.layer);

    std::int64_t collision_mask =
        static_cast<std::int64_t>(
            value.collision_mask);

    if (!read_bool(
            data,
            "Enabled",
            value.enabled) ||
        !read_bool(
            data,
            "Is Trigger",
            value.is_trigger) ||
        !read_integer(
            data,
            "Layer",
            layer,
            true) ||
        !read_integer(
            data,
            "Collision Mask",
            collision_mask,
            true) ||
        layer < 0 ||
        layer > 31 ||
        collision_mask < 0 ||
        collision_mask >
            static_cast<std::int64_t>(
                0xffffffffu) ||
        !read_float(
            data,
            "Friction",
            value.friction,
            true) ||
        !read_float(
            data,
            "Restitution",
            value.restitution,
            true) ||
        !valid_contact_material(
            value.friction,
            value.restitution) ||
        !read_vec3(
            data,
            "Center",
            value.center) ||
        !read_vec3(
            data,
            "Size",
            value.size) ||
        !valid_box_size(
            value.size,
            is_2d)) {

        if (error) {
            *error =
                "malformed " +
                std::string(type_name) +
                " data";
        }
        return false;
    }

    value.layer =
        static_cast<std::uint32_t>(
            layer);

    value.collision_mask =
        static_cast<std::uint32_t>(
            collision_mask);

    auto* component =
        ensure_component<T>(
            world,
            entity,
            type);

    if (!component) return false;

    *component = value;
    return true;
}

template <typename T>
std::optional<core::SerializedComponentData>
capture_radial(
    const core::World& world,
    core::Entity entity,
    core::ComponentTypeId type) {

    const auto* value =
        world.get_component<T>(
            entity,
            type);

    if (!value) {
        return std::nullopt;
    }

    core::SerializedComponentData data;
    data.properties = {
        bool_property(
            "Enabled",
            value->enabled),
        bool_property(
            "Is Trigger",
            value->is_trigger),
        integer_property(
            "Layer",
            static_cast<std::int64_t>(
                value->layer)),
        integer_property(
            "Collision Mask",
            static_cast<std::int64_t>(
                value->collision_mask)),
        float_property(
            "Friction",
            value->friction),
        float_property(
            "Restitution",
            value->restitution),
        vec3_property(
            "Center",
            value->center),
        float_property(
            "Radius",
            value->radius)
    };

    return data;
}

template <typename T>
bool restore_radial(
    core::World& world,
    core::Entity entity,
    core::ComponentTypeId type,
    const core::SerializedComponentData& data,
    std::string_view type_name,
    std::string* error) {

    T value;

    std::int64_t layer =
        static_cast<std::int64_t>(
            value.layer);

    std::int64_t collision_mask =
        static_cast<std::int64_t>(
            value.collision_mask);

    if (!read_bool(
            data,
            "Enabled",
            value.enabled) ||
        !read_bool(
            data,
            "Is Trigger",
            value.is_trigger) ||
        !read_integer(
            data,
            "Layer",
            layer,
            true) ||
        !read_integer(
            data,
            "Collision Mask",
            collision_mask,
            true) ||
        layer < 0 ||
        layer > 31 ||
        collision_mask < 0 ||
        collision_mask >
            static_cast<std::int64_t>(
                0xffffffffu) ||
        !read_float(
            data,
            "Friction",
            value.friction,
            true) ||
        !read_float(
            data,
            "Restitution",
            value.restitution,
            true) ||
        !valid_contact_material(
            value.friction,
            value.restitution) ||
        !read_vec3(
            data,
            "Center",
            value.center) ||
        !read_float(
            data,
            "Radius",
            value.radius) ||
        !valid_radius(
            value.radius)) {

        if (error) {
            *error =
                "malformed " +
                std::string(type_name) +
                " data";
        }
        return false;
    }

    value.layer =
        static_cast<std::uint32_t>(
            layer);

    value.collision_mask =
        static_cast<std::uint32_t>(
            collision_mask);

    auto* component =
        ensure_component<T>(
            world,
            entity,
            type);

    if (!component) return false;

    *component = value;
    return true;
}

std::optional<core::SerializedComponentData>
capture_capsule(
    const core::World& world,
    core::Entity entity) {

    const auto* value =
        world.get_component<CapsuleCollider>(
            entity,
            capsule_collider_type());

    if (!value) return std::nullopt;

    core::SerializedComponentData data;
    data.properties = {
        bool_property("Enabled", value->enabled),
        bool_property("Is Trigger", value->is_trigger),
        integer_property(
            "Layer",
            static_cast<std::int64_t>(value->layer)),
        integer_property(
            "Collision Mask",
            static_cast<std::int64_t>(
                value->collision_mask)),
        float_property("Friction", value->friction),
        float_property("Restitution", value->restitution),
        vec3_property("Center", value->center),
        float_property("Radius", value->radius),
        float_property("Height", value->height),
        integer_property(
            "Direction",
            static_cast<std::int64_t>(
                value->direction))
    };

    return data;
}

bool restore_capsule(
    core::World& world,
    core::Entity entity,
    const core::SerializedComponentData& data,
    std::string* error) {

    CapsuleCollider value;
    std::int64_t layer = value.layer;
    std::int64_t collision_mask =
        value.collision_mask;
    std::int64_t direction =
        value.direction;

    if (!read_bool(data, "Enabled", value.enabled) ||
        !read_bool(
            data,
            "Is Trigger",
            value.is_trigger) ||
        !read_integer(data, "Layer", layer, true) ||
        !read_integer(
            data,
            "Collision Mask",
            collision_mask,
            true) ||
        !read_float(
            data,
            "Friction",
            value.friction,
            true) ||
        !read_float(
            data,
            "Restitution",
            value.restitution,
            true) ||
        !read_vec3(data, "Center", value.center) ||
        !read_float(data, "Radius", value.radius) ||
        !read_float(data, "Height", value.height) ||
        !read_integer(
            data,
            "Direction",
            direction) ||
        layer < 0 ||
        layer > 31 ||
        collision_mask < 0 ||
        collision_mask >
            static_cast<std::int64_t>(
                0xffffffffu) ||
        direction < 0 ||
        direction > 2 ||
        !valid_contact_material(
            value.friction,
            value.restitution) ||
        !valid_capsule(
            value.radius,
            value.height,
            static_cast<std::uint32_t>(
                direction))) {

        if (error) {
            *error =
                "malformed NEngine.CapsuleCollider data";
        }
        return false;
    }

    value.layer =
        static_cast<std::uint32_t>(layer);
    value.collision_mask =
        static_cast<std::uint32_t>(
            collision_mask);
    value.direction =
        static_cast<std::uint32_t>(
            direction);

    auto* component =
        ensure_component<CapsuleCollider>(
            world,
            entity,
            capsule_collider_type());

    if (!component) return false;
    *component = value;
    return true;
}

std::optional<core::SerializedComponentData>
capture_capsule2d(
    const core::World& world,
    core::Entity entity) {

    const auto* value =
        world.get_component<CapsuleCollider2D>(
            entity,
            capsule_collider2d_type());

    if (!value) return std::nullopt;

    core::SerializedComponentData data;
    data.properties = {
        bool_property("Enabled", value->enabled),
        bool_property("Is Trigger", value->is_trigger),
        integer_property(
            "Layer",
            static_cast<std::int64_t>(value->layer)),
        integer_property(
            "Collision Mask",
            static_cast<std::int64_t>(
                value->collision_mask)),
        float_property("Friction", value->friction),
        float_property("Restitution", value->restitution),
        vec3_property("Center", value->center),
        vec3_property("Size", value->size),
        integer_property(
            "Direction",
            static_cast<std::int64_t>(
                value->direction))
    };

    return data;
}

bool restore_capsule2d(
    core::World& world,
    core::Entity entity,
    const core::SerializedComponentData& data,
    std::string* error) {

    CapsuleCollider2D value;
    std::int64_t layer = value.layer;
    std::int64_t collision_mask =
        value.collision_mask;
    std::int64_t direction =
        value.direction;

    if (!read_bool(data, "Enabled", value.enabled) ||
        !read_bool(
            data,
            "Is Trigger",
            value.is_trigger) ||
        !read_integer(data, "Layer", layer, true) ||
        !read_integer(
            data,
            "Collision Mask",
            collision_mask,
            true) ||
        !read_float(
            data,
            "Friction",
            value.friction,
            true) ||
        !read_float(
            data,
            "Restitution",
            value.restitution,
            true) ||
        !read_vec3(data, "Center", value.center) ||
        !read_vec3(data, "Size", value.size) ||
        !read_integer(
            data,
            "Direction",
            direction) ||
        layer < 0 ||
        layer > 31 ||
        collision_mask < 0 ||
        collision_mask >
            static_cast<std::int64_t>(
                0xffffffffu) ||
        direction < 0 ||
        direction > 1 ||
        !valid_contact_material(
            value.friction,
            value.restitution) ||
        !valid_capsule2d(
            value.size,
            static_cast<std::uint32_t>(
                direction))) {

        if (error) {
            *error =
                "malformed NEngine.CapsuleCollider2D data";
        }
        return false;
    }

    value.layer =
        static_cast<std::uint32_t>(layer);
    value.collision_mask =
        static_cast<std::uint32_t>(
            collision_mask);
    value.direction =
        static_cast<std::uint32_t>(
            direction);

    auto* component =
        ensure_component<CapsuleCollider2D>(
            world,
            entity,
            capsule_collider2d_type());

    if (!component) return false;
    *component = value;
    return true;
}

} // namespace

bool register_component_metadata(
    core::ComponentRegistry& registry) {

    bool ok = true;

    ok =
        register_rigidbody_metadata(
            registry,
            "NEngine.Rigidbody",
            rigidbody_type(),
            "Physics") &&
        ok;

    ok =
        register_box_metadata(
            registry,
            "NEngine.BoxCollider",
            box_collider_type(),
            "Physics") &&
        ok;

    ok =
        register_radial_metadata(
            registry,
            "NEngine.SphereCollider",
            sphere_collider_type(),
            "Physics") &&
        ok;

    ok =
        register_capsule_metadata(
            registry,
            "NEngine.CapsuleCollider",
            capsule_collider_type(),
            "Physics",
            false) &&
        ok;

    ok =
        register_rigidbody_metadata(
            registry,
            "NEngine.Rigidbody2D",
            rigidbody2d_type(),
            "Physics 2D") &&
        ok;

    ok =
        register_box_metadata(
            registry,
            "NEngine.BoxCollider2D",
            box_collider2d_type(),
            "Physics 2D") &&
        ok;

    ok =
        register_radial_metadata(
            registry,
            "NEngine.CircleCollider2D",
            circle_collider2d_type(),
            "Physics 2D") &&
        ok;

    ok =
        register_capsule_metadata(
            registry,
            "NEngine.CapsuleCollider2D",
            capsule_collider2d_type(),
            "Physics 2D",
            true) &&
        ok;

    return ok;
}

bool register_component_serializers(
    core::ComponentSerializationRegistry& registry) {

    bool ok = true;

    ok =
        registry.register_codec({
            rigidbody_type(),
            2,
            "NEngine.Rigidbody",
            [](const core::World& world,
               core::Entity entity) {
                return capture_rigidbody<Rigidbody>(
                    world,
                    entity,
                    rigidbody_type());
            },
            [](core::World& world,
               core::Entity entity,
               const core::SerializedComponentData& data,
               std::string* error) {
                return restore_rigidbody<Rigidbody>(
                    world,
                    entity,
                    rigidbody_type(),
                    data,
                    "NEngine.Rigidbody",
                    error);
            }
        }) &&
        ok;

    ok =
        registry.register_codec({
            box_collider_type(),
            2,
            "NEngine.BoxCollider",
            [](const core::World& world,
               core::Entity entity) {
                return capture_box<BoxCollider>(
                    world,
                    entity,
                    box_collider_type());
            },
            [](core::World& world,
               core::Entity entity,
               const core::SerializedComponentData& data,
               std::string* error) {
                return restore_box<BoxCollider>(
                    world,
                    entity,
                    box_collider_type(),
                    data,
                    false,
                    "NEngine.BoxCollider",
                    error);
            }
        }) &&
        ok;

    ok =
        registry.register_codec({
            sphere_collider_type(),
            2,
            "NEngine.SphereCollider",
            [](const core::World& world,
               core::Entity entity) {
                return capture_radial<SphereCollider>(
                    world,
                    entity,
                    sphere_collider_type());
            },
            [](core::World& world,
               core::Entity entity,
               const core::SerializedComponentData& data,
               std::string* error) {
                return restore_radial<SphereCollider>(
                    world,
                    entity,
                    sphere_collider_type(),
                    data,
                    "NEngine.SphereCollider",
                    error);
            }
        }) &&
        ok;

    ok =
        registry.register_codec({
            capsule_collider_type(),
            1,
            "NEngine.CapsuleCollider",
            [](const core::World& world,
               core::Entity entity) {
                return capture_capsule(
                    world,
                    entity);
            },
            [](core::World& world,
               core::Entity entity,
               const core::SerializedComponentData& data,
               std::string* error) {
                return restore_capsule(
                    world,
                    entity,
                    data,
                    error);
            }
        }) &&
        ok;

    ok =
        registry.register_codec({
            rigidbody2d_type(),
            2,
            "NEngine.Rigidbody2D",
            [](const core::World& world,
               core::Entity entity) {
                return capture_rigidbody<Rigidbody2D>(
                    world,
                    entity,
                    rigidbody2d_type());
            },
            [](core::World& world,
               core::Entity entity,
               const core::SerializedComponentData& data,
               std::string* error) {
                return restore_rigidbody<Rigidbody2D>(
                    world,
                    entity,
                    rigidbody2d_type(),
                    data,
                    "NEngine.Rigidbody2D",
                    error);
            }
        }) &&
        ok;

    ok =
        registry.register_codec({
            box_collider2d_type(),
            2,
            "NEngine.BoxCollider2D",
            [](const core::World& world,
               core::Entity entity) {
                return capture_box<BoxCollider2D>(
                    world,
                    entity,
                    box_collider2d_type());
            },
            [](core::World& world,
               core::Entity entity,
               const core::SerializedComponentData& data,
               std::string* error) {
                return restore_box<BoxCollider2D>(
                    world,
                    entity,
                    box_collider2d_type(),
                    data,
                    true,
                    "NEngine.BoxCollider2D",
                    error);
            }
        }) &&
        ok;

    ok =
        registry.register_codec({
            circle_collider2d_type(),
            2,
            "NEngine.CircleCollider2D",
            [](const core::World& world,
               core::Entity entity) {
                return capture_radial<CircleCollider2D>(
                    world,
                    entity,
                    circle_collider2d_type());
            },
            [](core::World& world,
               core::Entity entity,
               const core::SerializedComponentData& data,
               std::string* error) {
                return restore_radial<CircleCollider2D>(
                    world,
                    entity,
                    circle_collider2d_type(),
                    data,
                    "NEngine.CircleCollider2D",
                    error);
            }
        }) &&
        ok;

    ok =
        registry.register_codec({
            capsule_collider2d_type(),
            1,
            "NEngine.CapsuleCollider2D",
            [](const core::World& world,
               core::Entity entity) {
                return capture_capsule2d(
                    world,
                    entity);
            },
            [](core::World& world,
               core::Entity entity,
               const core::SerializedComponentData& data,
               std::string* error) {
                return restore_capsule2d(
                    world,
                    entity,
                    data,
                    error);
            }
        }) &&
        ok;

    return ok;
}

} // namespace nengine::physics
