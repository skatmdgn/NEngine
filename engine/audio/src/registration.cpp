#include "nengine/audio/registration.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "nengine/audio/components.hpp"

namespace nengine::audio {
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
    bool& value) {

    const auto* property =
        find_property(data, name);

    if (!property) return false;

    const auto* typed =
        std::get_if<bool>(&property->value);

    if (!typed) return false;

    value = *typed;
    return true;
}

bool read_float(
    const core::SerializedComponentData& data,
    std::string_view name,
    float& value) {

    const auto* property =
        find_property(data, name);

    if (!property) return false;

    const auto* typed =
        std::get_if<double>(&property->value);

    if (!typed) return false;

    value = static_cast<float>(*typed);
    return true;
}

bool read_asset(
    const core::SerializedComponentData& data,
    std::string_view name,
    assets::AssetGuid& value) {

    const auto* property =
        find_property(data, name);

    if (!property) return false;

    const auto* typed =
        std::get_if<std::string>(
            &property->value);

    if (!typed) return false;

    if (typed->empty()) {
        value = {};
        return true;
    }

    const auto parsed =
        assets::AssetGuid::parse(*typed);

    if (!parsed) return false;

    value = *parsed;
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

core::SerializedPropertyData asset_property(
    std::string name,
    assets::AssetGuid value) {

    return {
        std::move(name),
        core::PropertyKind::AssetReference,
        core::PropertyValue{
            value.valid()
                ? value.to_string()
                : std::string{}}
    };
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

bool valid_source(
    const AudioSource& value) noexcept {

    return value.volume >= 0.0f &&
           value.volume <= 1.0f &&
           value.pitch >= 0.0f &&
           value.pan_stereo >= -1.0f &&
           value.pan_stereo <= 1.0f &&
           value.min_distance > 0.0f &&
           value.max_distance >=
               value.min_distance;
}

bool valid_listener(
    const AudioListener& value) noexcept {

    return value.volume >= 0.0f &&
           value.volume <= 1.0f;
}

} // namespace

bool register_component_metadata(
    core::ComponentRegistry& registry) {

    bool ok =
        registry.register_type(
            "NEngine.AudioSource",
            "Audio",
            true,
            false);

    const auto flags =
        core::PropertyFlags::Serializable |
        core::PropertyFlags::Editable;

    for (const auto& property :
         {
            core::PropertyDescriptor{
                "Enabled",
                core::PropertyKind::Boolean,
                flags},
            core::PropertyDescriptor{
                "Clip",
                core::PropertyKind::AssetReference,
                flags},
            core::PropertyDescriptor{
                "Play On Awake",
                core::PropertyKind::Boolean,
                flags},
            core::PropertyDescriptor{
                "Loop",
                core::PropertyKind::Boolean,
                flags},
            core::PropertyDescriptor{
                "Spatialize",
                core::PropertyKind::Boolean,
                flags},
            core::PropertyDescriptor{
                "Volume",
                core::PropertyKind::Float,
                flags},
            core::PropertyDescriptor{
                "Pitch",
                core::PropertyKind::Float,
                flags},
            core::PropertyDescriptor{
                "Pan Stereo",
                core::PropertyKind::Float,
                flags},
            core::PropertyDescriptor{
                "Min Distance",
                core::PropertyKind::Float,
                flags},
            core::PropertyDescriptor{
                "Max Distance",
                core::PropertyKind::Float,
                flags}}) {

        ok =
            registry.register_property(
                audio_source_type(),
                property) &&
            ok;
    }

    ok =
        registry.register_type(
            "NEngine.AudioListener",
            "Audio",
            true,
            false) &&
        ok;

    ok =
        registry.register_property(
            audio_listener_type(),
            {
                "Enabled",
                core::PropertyKind::Boolean,
                flags
            }) &&
        ok;

    ok =
        registry.register_property(
            audio_listener_type(),
            {
                "Volume",
                core::PropertyKind::Float,
                flags
            }) &&
        ok;

    return ok;
}

bool register_component_serializers(
    core::ComponentSerializationRegistry& registry) {

    bool ok = true;

    ok =
        registry.register_codec({
            audio_source_type(),
            1,
            "NEngine.AudioSource",
            [](const core::World& world,
               core::Entity entity)
                -> std::optional<
                    core::SerializedComponentData> {

                const auto* source =
                    world.get_component<
                        AudioSource>(
                            entity,
                            audio_source_type());

                if (!source) {
                    return std::nullopt;
                }

                core::SerializedComponentData data;
                data.properties = {
                    bool_property(
                        "Enabled",
                        source->enabled),
                    asset_property(
                        "Clip",
                        source->clip),
                    bool_property(
                        "Play On Awake",
                        source->play_on_awake),
                    bool_property(
                        "Loop",
                        source->loop),
                    bool_property(
                        "Spatialize",
                        source->spatialize),
                    float_property(
                        "Volume",
                        source->volume),
                    float_property(
                        "Pitch",
                        source->pitch),
                    float_property(
                        "Pan Stereo",
                        source->pan_stereo),
                    float_property(
                        "Min Distance",
                        source->min_distance),
                    float_property(
                        "Max Distance",
                        source->max_distance)
                };

                return data;
            },
            [](core::World& world,
               core::Entity entity,
               const core::SerializedComponentData& data,
               std::string* error) {

                AudioSource value;

                if (!read_bool(
                        data,
                        "Enabled",
                        value.enabled) ||
                    !read_asset(
                        data,
                        "Clip",
                        value.clip) ||
                    !read_bool(
                        data,
                        "Play On Awake",
                        value.play_on_awake) ||
                    !read_bool(
                        data,
                        "Loop",
                        value.loop) ||
                    !read_bool(
                        data,
                        "Spatialize",
                        value.spatialize) ||
                    !read_float(
                        data,
                        "Volume",
                        value.volume) ||
                    !read_float(
                        data,
                        "Pitch",
                        value.pitch) ||
                    !read_float(
                        data,
                        "Pan Stereo",
                        value.pan_stereo) ||
                    !read_float(
                        data,
                        "Min Distance",
                        value.min_distance) ||
                    !read_float(
                        data,
                        "Max Distance",
                        value.max_distance) ||
                    !valid_source(value)) {

                    if (error) {
                        *error =
                            "malformed NEngine.AudioSource data";
                    }
                    return false;
                }

                value.playing = false;
                value.time_seconds = 0.0f;

                auto* source =
                    ensure_component<AudioSource>(
                        world,
                        entity,
                        audio_source_type());

                if (!source) return false;

                *source = value;
                return true;
            }
        }) &&
        ok;

    ok =
        registry.register_codec({
            audio_listener_type(),
            1,
            "NEngine.AudioListener",
            [](const core::World& world,
               core::Entity entity)
                -> std::optional<
                    core::SerializedComponentData> {

                const auto* listener =
                    world.get_component<
                        AudioListener>(
                            entity,
                            audio_listener_type());

                if (!listener) {
                    return std::nullopt;
                }

                core::SerializedComponentData data;
                data.properties = {
                    bool_property(
                        "Enabled",
                        listener->enabled),
                    float_property(
                        "Volume",
                        listener->volume)
                };

                return data;
            },
            [](core::World& world,
               core::Entity entity,
               const core::SerializedComponentData& data,
               std::string* error) {

                AudioListener value;

                if (!read_bool(
                        data,
                        "Enabled",
                        value.enabled) ||
                    !read_float(
                        data,
                        "Volume",
                        value.volume) ||
                    !valid_listener(value)) {

                    if (error) {
                        *error =
                            "malformed NEngine.AudioListener data";
                    }
                    return false;
                }

                auto* listener =
                    ensure_component<AudioListener>(
                        world,
                        entity,
                        audio_listener_type());

                if (!listener) return false;

                *listener = value;
                return true;
            }
        }) &&
        ok;

    return ok;
}

} // namespace nengine::audio
