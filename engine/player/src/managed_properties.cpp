#include "nengine/player/managed_properties.hpp"
#include "nengine/scripting/managed_runtime.hpp"
#include "nengine/physics/components.hpp"
#include <cmath>
#include <variant>
namespace nengine::player {
bool read_player_transform_property(const core::World& world,
    core::Entity entity, std::string_view component,
    std::string_view property, core::PropertyValue& value) {
    if (component == "NEngine.Rigidbody2D") {
        const auto* body = world.get_component<physics::Rigidbody2D>(
            entity, physics::rigidbody2d_type());
        if (!body) return false;
        if (property == "Mass") value = static_cast<double>(body->mass);
        else if (property == "Enabled") value = body->enabled;
        else if (property == "Linear Velocity") value = body->linear_velocity;
        else return false;
        return true;
    }
    if (component != "NEngine.Transform") return false;
    const auto* transform = world.transform(entity);
    if (!transform) return false;
    if (property == "Local Position") value = transform->local_position;
    else if (property == "Local Rotation") value = transform->local_rotation;
    else if (property == "Local Scale") value = transform->local_scale;
    else return false;
    return true;
}
bool write_player_transform_property(core::World& world,
    core::Entity entity, std::string_view component,
    std::string_view property, const core::PropertyValue& value) {
    if (component == "NEngine.Rigidbody2D") {
        auto* body = world.get_component<physics::Rigidbody2D>(
            entity, physics::rigidbody2d_type());
        if (!body) return false;
        if (property == "Mass") {
            const auto* v = std::get_if<double>(&value);
            if (!v || !std::isfinite(*v) || *v <= 0.0 ||
                *v > 1.0e9) return false;
            body->mass = static_cast<float>(*v);
        } else if (property == "Enabled") {
            const auto* v = std::get_if<bool>(&value);
            if (!v) return false;
            body->enabled = *v;
        } else if (property == "Linear Velocity") {
            const auto* v = std::get_if<core::Vec3>(&value);
            if (!v || !std::isfinite(v->x) || !std::isfinite(v->y) ||
                !std::isfinite(v->z)) return false;
            body->linear_velocity = *v;
        } else return false;
        return true;
    }
    if (component != "NEngine.Transform") return false;
    auto* transform = world.transform(entity);
    if (!transform) return false;
    if (property == "Local Rotation") {
        const auto* v = std::get_if<core::Quat>(&value);
        if (!v || !std::isfinite(v->x) || !std::isfinite(v->y) ||
            !std::isfinite(v->z) || !std::isfinite(v->w)) return false;
        transform->local_rotation = *v;
        return true;
    }
    const auto* v = std::get_if<core::Vec3>(&value);
    if (!v || !std::isfinite(v->x) || !std::isfinite(v->y) ||
        !std::isfinite(v->z)) return false;
    if (property == "Local Position") transform->local_position = *v;
    else if (property == "Local Scale") transform->local_scale = *v;
    else return false;
    return true;
}
void bind_player_properties(scripting::ManagedRuntime& runtime) {
    runtime.bind_property_access(nullptr,
        [](void*, const core::World& world, core::Entity entity,
           std::string_view component, std::string_view property,
           core::PropertyValue& value) {
            return read_player_transform_property(
                world, entity, component, property, value);
        },
        [](void*, core::World& world, core::Entity entity,
           std::string_view component, std::string_view property,
           const core::PropertyValue& value) {
            return write_player_transform_property(
                world, entity, component, property, value);
        });
}
}
