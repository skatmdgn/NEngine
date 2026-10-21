#include "managed_audio_properties.hpp"

#include <cmath>
#include <limits>
#include <string>
#include <variant>

#include "nengine/assets/asset_guid.hpp"
#include "nengine/audio/components.hpp"

namespace nengine::player {
namespace {

template <typename Component>
bool boolean_field(Component& component,
                   bool Component::* member,
                   const core::PropertyValue& value) {
    const auto* flag = std::get_if<bool>(&value);
    if (!flag) return false;
    component.*member = *flag;
    return true;
}

bool number(const core::PropertyValue& value, float& result,
            double minimum, double maximum, bool strictly_positive = false) {
    const auto* n = std::get_if<double>(&value);
    if (!n || !std::isfinite(*n) || *n < minimum || *n > maximum ||
        (strictly_positive && *n <= 0.0)) return false;
    const float converted = static_cast<float>(*n);
    if (!std::isfinite(converted) ||
        (strictly_positive && converted <= 0.0f))
        return false;
    result = converted;
    return true;
}

bool parse_guid(const core::PropertyValue& value, assets::AssetGuid& guid) {
    const auto* text = std::get_if<std::string>(&value);
    if (!text) return false;
    if (text->empty()) {
        guid = {};
        return true;
    }
    const auto parsed = assets::AssetGuid::parse(*text);
    if (!parsed || !parsed->valid()) return false;
    guid = *parsed;
    return true;
}

bool read_source(const audio::AudioSource& s, std::string_view key,
                 core::PropertyValue& out) {
    if (key == "Enabled") out = s.enabled;
    else if (key == "Play On Awake") out = s.play_on_awake;
    else if (key == "Loop") out = s.loop;
    else if (key == "Spatialize") out = s.spatialize;
    else if (key == "Playing") out = s.playing;
    else if (key == "Clip")
        out = s.clip.valid() ? s.clip.to_string() : std::string{};
    else if (key == "Volume") out = static_cast<double>(s.volume);
    else if (key == "Pitch") out = static_cast<double>(s.pitch);
    else if (key == "Pan Stereo") out = static_cast<double>(s.pan_stereo);
    else if (key == "Min Distance")
        out = static_cast<double>(s.min_distance);
    else if (key == "Max Distance")
        out = static_cast<double>(s.max_distance);
    else if (key == "Time") out = static_cast<double>(s.time_seconds);
    else return false;
    return true;
}

bool write_source(audio::AudioSource& s, std::string_view key,
                  const core::PropertyValue& value) {
    if (key == "Enabled")
        return boolean_field(s, &audio::AudioSource::enabled, value);
    if (key == "Play On Awake")
        return boolean_field(s, &audio::AudioSource::play_on_awake, value);
    if (key == "Loop")
        return boolean_field(s, &audio::AudioSource::loop, value);
    if (key == "Spatialize")
        return boolean_field(s, &audio::AudioSource::spatialize, value);
    if (key == "Playing")
        return boolean_field(s, &audio::AudioSource::playing, value);
    if (key == "Clip") {
        assets::AssetGuid next;
        if (!parse_guid(value, next)) return false;
        if (next != s.clip) {
            s.clip = next;
            s.time_seconds = 0.f;
            s.playing = false;
        }
        return true;
    }
    float n{};
    if (key == "Volume") {
        if (!number(value, n, 0.0, 1.0)) return false;
        s.volume = n;
    } else if (key == "Pitch") {
        if (!number(value, n, 0.0, 16.0)) return false;
        s.pitch = n;
    } else if (key == "Pan Stereo") {
        if (!number(value, n, -1.0, 1.0)) return false;
        s.pan_stereo = n;
    } else if (key == "Min Distance") {
        if (!number(value, n, 0.0, s.max_distance, true))
            return false;
        s.min_distance = n;
    } else if (key == "Max Distance") {
        if (!number(value, n, s.min_distance, 1.0e9)) return false;
        s.max_distance = n;
    } else if (key == "Time") {
        if (!number(value, n, 0.0, 1.0e9)) return false;
        s.time_seconds = n;
    } else return false;
    return true;
}

bool read_listener(const audio::AudioListener& l, std::string_view key,
                   core::PropertyValue& out) {
    if (key == "Enabled") out = l.enabled;
    else if (key == "Volume") out = static_cast<double>(l.volume);
    else return false;
    return true;
}

bool write_listener(audio::AudioListener& l, std::string_view key,
                    const core::PropertyValue& value) {
    if (key == "Enabled")
        return boolean_field(l, &audio::AudioListener::enabled, value);
    if (key == "Volume") {
        float n{};
        if (!number(value, n, 0.0, 1.0)) return false;
        l.volume = n;
        return true;
    }
    return false;
}

} // namespace

bool read_player_audio_property(const core::World& world,
    core::Entity entity, std::string_view component,
    std::string_view property, core::PropertyValue& value) {
    if (component == "NEngine.AudioSource") {
        const auto* s = world.get_component<audio::AudioSource>(
            entity, audio::audio_source_type());
        return s && read_source(*s, property, value);
    }
    if (component == "NEngine.AudioListener") {
        const auto* l = world.get_component<audio::AudioListener>(
            entity, audio::audio_listener_type());
        return l && read_listener(*l, property, value);
    }
    return false;
}

bool write_player_audio_property(core::World& world,
    core::Entity entity, std::string_view component,
    std::string_view property, const core::PropertyValue& value) {
    if (component == "NEngine.AudioSource") {
        auto* s = world.get_component<audio::AudioSource>(
            entity, audio::audio_source_type());
        return s && write_source(*s, property, value);
    }
    if (component == "NEngine.AudioListener") {
        auto* l = world.get_component<audio::AudioListener>(
            entity, audio::audio_listener_type());
        return l && write_listener(*l, property, value);
    }
    return false;
}
} // namespace nengine::player
