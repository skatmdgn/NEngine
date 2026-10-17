#include "nengine/player/managed_properties.hpp"
#include "nengine/scripting/managed_runtime.hpp"
#include "nengine/physics/components.hpp"
#include "nengine/audio/components.hpp"

#include <cmath>
#include <variant>

namespace nengine::player {
namespace {

bool finite_vector(const core::Vec3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

template <typename Body>
bool read_rigidbody_property(const Body& body, std::string_view property,
                             core::PropertyValue& value) {
    if (property == "Enabled") value = body.enabled;
    else if (property == "Use Gravity") value = body.use_gravity;
    else if (property == "Is Kinematic") value = body.is_kinematic;
    else if (property == "Allow Sleep") value = body.allow_sleep;
    else if (property == "Sleeping") value = body.sleeping;
    else if (property == "Mass") value = static_cast<double>(body.mass);
    else if (property == "Gravity Scale")
        value = static_cast<double>(body.gravity_scale);
    else if (property == "Sleep Threshold")
        value = static_cast<double>(body.sleep_threshold);
    else if (property == "Linear Velocity") value = body.linear_velocity;
    else if (property == "Angular Velocity") value = body.angular_velocity;
    else return false;
    return true;
}

template <typename Body>
bool write_rigidbody_property(Body& body, std::string_view property,
                              const core::PropertyValue& value) {
    if (property == "Enabled" || property == "Use Gravity" ||
        property == "Is Kinematic" || property == "Allow Sleep" ||
        property == "Sleeping") {
        const auto* enabled = std::get_if<bool>(&value);
        if (!enabled) return false;
        if (property == "Enabled") body.enabled = *enabled;
        else if (property == "Use Gravity") body.use_gravity = *enabled;
        else if (property == "Is Kinematic") body.is_kinematic = *enabled;
        else if (property == "Allow Sleep") {
            body.allow_sleep = *enabled;
            if (!*enabled) {
                body.sleeping = false;
                body.sleep_timer = 0.f;
            }
        } else {
            body.sleeping = *enabled && body.allow_sleep;
            body.sleep_timer = 0.f;
            if (body.sleeping) {
                body.linear_velocity = {};
                body.angular_velocity = {};
            }
        }
        return true;
    }
    if (property == "Mass" || property == "Gravity Scale" ||
        property == "Sleep Threshold") {
        const auto* number = std::get_if<double>(&value);
        if (!number || !std::isfinite(*number) || *number > 1.0e9 ||
            (property == "Mass" ? *number <= 0.0 : *number < 0.0))
            return false;
        const auto converted = static_cast<float>(*number);
        if (property == "Mass") {
            if (converted <= 0.0f) return false; // reject float underflow
            body.mass = converted;
        } else if (property == "Gravity Scale") {
            body.gravity_scale = converted;
        } else body.sleep_threshold = converted;
        return true;
    }
    if (property == "Linear Velocity" ||
        property == "Angular Velocity") {
        const auto* vector = std::get_if<core::Vec3>(&value);
        if (!vector || !finite_vector(*vector)) return false;
        if (property == "Linear Velocity") body.linear_velocity = *vector;
        else body.angular_velocity = *vector;
        body.sleeping = false;
        body.sleep_timer = 0.f;
        return true;
    }
    return false;
}

} // namespace

bool read_player_transform_property(const core::World& world,
    core::Entity entity, std::string_view component,
    std::string_view property, core::PropertyValue& value) {
    if (component == "NEngine.AudioSource") {
        const auto* audio = world.get_component<audio::AudioSource>(
            entity, audio::audio_source_type());
        if (!audio) return false;
        if (property == "Volume") value = static_cast<double>(audio->volume);
        else if (property == "Loop") value = audio->loop;
        else if (property == "Enabled") value = audio->enabled;
        else if (property == "Playing") value = audio->playing;
        else return false;
        return true;
    }
    if (component == "NEngine.Rigidbody") {
        const auto* body = world.get_component<physics::Rigidbody>(
            entity, physics::rigidbody_type());
        return body && read_rigidbody_property(*body, property, value);
    }
    if (component == "NEngine.Rigidbody2D") {
        const auto* body = world.get_component<physics::Rigidbody2D>(
            entity, physics::rigidbody2d_type());
        return body && read_rigidbody_property(*body, property, value);
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
    if (component == "NEngine.AudioSource") {
        auto* audio = world.get_component<audio::AudioSource>(
            entity, audio::audio_source_type());
        if (!audio) return false;
        if (property == "Volume") {
            const auto* v = std::get_if<double>(&value);
            if (!v || !std::isfinite(*v) || *v < 0.0 || *v > 1.0)
                return false;
            audio->volume = static_cast<float>(*v);
            return true;
        }
        const auto* v = std::get_if<bool>(&value);
        if (!v) return false;
        if (property == "Loop") audio->loop = *v;
        else if (property == "Enabled") audio->enabled = *v;
        else if (property == "Playing") audio->playing = *v;
        else return false;
        return true;
    }
    if (component == "NEngine.Rigidbody") {
        auto* body = world.get_component<physics::Rigidbody>(
            entity, physics::rigidbody_type());
        return body && write_rigidbody_property(*body, property, value);
    }
    if (component == "NEngine.Rigidbody2D") {
        auto* body = world.get_component<physics::Rigidbody2D>(
            entity, physics::rigidbody2d_type());
        return body && write_rigidbody_property(*body, property, value);
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
    if (!v || !finite_vector(*v)) return false;
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

} // namespace nengine::player
