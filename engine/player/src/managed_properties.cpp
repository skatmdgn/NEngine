#include "nengine/player/managed_properties.hpp"
#include "nengine/scripting/managed_runtime.hpp"
#include "nengine/physics/components.hpp"
#include "nengine/audio/components.hpp"
#include <cmath>
#include <limits>
#include <variant>

namespace nengine::player {
namespace {
bool assign_finite_float(const core::PropertyValue& value, float& destination,
                         double minimum, double maximum) {
    const auto* number = std::get_if<double>(&value);
    if (!number || !std::isfinite(*number) ||
        *number < minimum || *number > maximum) return false;
    destination = static_cast<float>(*number);
    return true;
}

bool finite_vector(const core::Vec3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

template <typename Body>
bool read_body(const Body& body, std::string_view property,
               core::PropertyValue& value) {
    if (property == "Enabled") value = body.enabled;
    else if (property == "Use Gravity") value = body.use_gravity;
    else if (property == "Is Kinematic") value = body.is_kinematic;
    else if (property == "Allow Sleep") value = body.allow_sleep;
    else if (property == "Sleeping") value = body.sleeping;
    else if (property == "Mass") value = static_cast<double>(body.mass);
    else if (property == "Gravity Scale") value = static_cast<double>(body.gravity_scale);
    else if (property == "Sleep Threshold") value = static_cast<double>(body.sleep_threshold);
    else if (property == "Linear Velocity") value = body.linear_velocity;
    else if (property == "Angular Velocity") value = body.angular_velocity;
    else return false;
    return true;
}

template <typename Body>
bool write_body(Body& body, std::string_view property,
                const core::PropertyValue& value) {
    if (property == "Enabled" || property == "Use Gravity" ||
        property == "Is Kinematic" || property == "Allow Sleep" ||
        property == "Sleeping") {
        const auto* flag = std::get_if<bool>(&value);
        if (!flag) return false;
        if (property == "Enabled") body.enabled = *flag;
        else if (property == "Use Gravity") body.use_gravity = *flag;
        else if (property == "Is Kinematic") body.is_kinematic = *flag;
        else if (property == "Allow Sleep") {
            body.allow_sleep = *flag;
            if (!*flag) {
                body.sleeping = false;
                body.sleep_timer = 0.0f;
            }
        } else {
            body.sleeping = *flag && body.allow_sleep;
            body.sleep_timer = 0.0f;
            if (body.sleeping) {
                body.linear_velocity = {};
                body.angular_velocity = {};
            }
        }
        return true;
    }
    if (property == "Mass")
        return assign_finite_float(value, body.mass, 0.000000000001, 1.0e9);
    if (property == "Gravity Scale")
        return assign_finite_float(value, body.gravity_scale, 0.0, 1.0e9);
    if (property == "Sleep Threshold")
        return assign_finite_float(value, body.sleep_threshold, 0.0, 1.0e9);
    if (property == "Linear Velocity" || property == "Angular Velocity") {
        const auto* vector = std::get_if<core::Vec3>(&value);
        if (!vector || !finite_vector(*vector)) return false;
        if (property == "Linear Velocity") body.linear_velocity = *vector;
        else body.angular_velocity = *vector;
        body.sleeping = false;
        body.sleep_timer = 0.0f;
        return true;
    }
    return false;
}

bool read_audio(const audio::AudioSource& source, std::string_view property,
                core::PropertyValue& value) {
    if (property == "Enabled") value = source.enabled;
    else if (property == "Clip") value = source.clip.to_string();
    else if (property == "Play On Awake") value = source.play_on_awake;
    else if (property == "Loop") value = source.loop;
    else if (property == "Spatialize") value = source.spatialize;
    else if (property == "Playing") value = source.playing;
    else if (property == "Volume") value = static_cast<double>(source.volume);
    else if (property == "Pitch") value = static_cast<double>(source.pitch);
    else if (property == "Pan Stereo") value = static_cast<double>(source.pan_stereo);
    else if (property == "Min Distance") value = static_cast<double>(source.min_distance);
    else if (property == "Max Distance") value = static_cast<double>(source.max_distance);
    else if (property == "Time") value = static_cast<double>(source.time_seconds);
    else return false;
    return true;
}

bool write_audio(audio::AudioSource& source, std::string_view property,
                 const core::PropertyValue& value) {
    if (property == "Clip") {
        const auto* text = std::get_if<std::string>(&value);
        if (!text) return false;
        const auto guid = assets::AssetGuid::parse(*text);
        if (!guid) return false;
        if (source.clip != *guid) {
            source.clip = *guid;
            source.playing = false;
            source.time_seconds = 0.0f;
        }
        return true;
    }
    if (property == "Enabled" || property == "Play On Awake" ||
        property == "Loop" || property == "Spatialize" ||
        property == "Playing") {
        const auto* flag = std::get_if<bool>(&value);
        if (!flag) return false;
        if (property == "Enabled") source.enabled = *flag;
        else if (property == "Play On Awake") source.play_on_awake = *flag;
        else if (property == "Loop") source.loop = *flag;
        else if (property == "Spatialize") source.spatialize = *flag;
        else source.playing = *flag;
        return true;
    }
    if (property == "Volume")
        return assign_finite_float(value, source.volume, 0.0, 1.0);
    if (property == "Pitch")
        return assign_finite_float(value, source.pitch, 0.0, 16.0);
    if (property == "Pan Stereo")
        return assign_finite_float(value, source.pan_stereo, -1.0, 1.0);
    if (property == "Min Distance")
        return assign_finite_float(value, source.min_distance,
                                   0.000000000001, source.max_distance);
    if (property == "Max Distance")
        return assign_finite_float(value, source.max_distance,
                                   source.min_distance, 1.0e9);
    if (property == "Time")
        return assign_finite_float(value, source.time_seconds, 0.0,
                                   std::numeric_limits<float>::max());
    return false;
}
} // namespace

bool read_player_transform_property(const core::World& world,
    core::Entity entity, std::string_view component,
    std::string_view property, core::PropertyValue& value) {
    if (component == "NEngine.Rigidbody") {
        const auto* body = world.get_component<physics::Rigidbody>(
            entity, physics::rigidbody_type());
        return body && read_body(*body, property, value);
    }
    if (component == "NEngine.Rigidbody2D") {
        const auto* body = world.get_component<physics::Rigidbody2D>(
            entity, physics::rigidbody2d_type());
        return body && read_body(*body, property, value);
    }
    if (component == "NEngine.AudioSource") {
        const auto* source = world.get_component<audio::AudioSource>(
            entity, audio::audio_source_type());
        return source && read_audio(*source, property, value);
    }
    if (component == "NEngine.AudioListener") {
        const auto* listener = world.get_component<audio::AudioListener>(
            entity, audio::audio_listener_type());
        if (!listener) return false;
        if (property == "Enabled") value = listener->enabled;
        else if (property == "Volume") value = static_cast<double>(listener->volume);
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
    if (component == "NEngine.Rigidbody") {
        auto* body = world.get_component<physics::Rigidbody>(
            entity, physics::rigidbody_type());
        return body && write_body(*body, property, value);
    }
    if (component == "NEngine.Rigidbody2D") {
        auto* body = world.get_component<physics::Rigidbody2D>(
            entity, physics::rigidbody2d_type());
        return body && write_body(*body, property, value);
    }
    if (component == "NEngine.AudioSource") {
        auto* source = world.get_component<audio::AudioSource>(
            entity, audio::audio_source_type());
        return source && write_audio(*source, property, value);
    }
    if (component == "NEngine.AudioListener") {
        auto* listener = world.get_component<audio::AudioListener>(
            entity, audio::audio_listener_type());
        if (!listener) return false;
        if (property == "Enabled") {
            const auto* flag = std::get_if<bool>(&value);
            if (!flag) return false;
            listener->enabled = *flag;
            return true;
        }
        if (property == "Volume")
            return assign_finite_float(value, listener->volume, 0.0, 1.0);
        return false;
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
