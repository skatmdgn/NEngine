#include "nengine/player/managed_properties.hpp"
#include "nengine/scripting/managed_runtime.hpp"
#include <cmath>
#include <variant>
namespace nengine::player {
bool read_player_transform_property(const core::World& world,
    core::Entity entity, std::string_view component,
    std::string_view property, core::PropertyValue& value) {
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
