#include "nengine/editor/audio_integration.hpp"

#include <optional>
#include <string>
#include <variant>

#include "nengine/audio/components.hpp"
#include "nengine/audio/registration.hpp"

namespace nengine::editor {
namespace {

std::string asset_reference_text(
    assets::AssetGuid guid) {

    return guid.valid()
        ? guid.to_string()
        : std::string{};
}

bool parse_asset_reference(
    const core::PropertyValue& value,
    assets::AssetGuid& guid) {

    const auto* typed =
        std::get_if<std::string>(&value);

    if (!typed) return false;

    if (typed->empty()) {
        guid = {};
        return true;
    }

    const auto parsed =
        assets::AssetGuid::parse(*typed);

    if (!parsed) return false;

    guid = *parsed;
    return true;
}

template <typename Component, typename Getter>
std::optional<core::PropertyValue>
read_component_property(
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

bool register_source_properties(
    PropertyAccessRegistry& properties) {

    bool ok = true;
    const auto type =
        audio::audio_source_type();

    const auto bool_property =
        [&properties, type, &ok](
            const char* name,
            bool audio::AudioSource::* member) {

            ok =
                properties.register_property(
                    type,
                    name,
                    core::PropertyKind::Boolean,
                    [type, member](
                        const core::World& world,
                        core::Entity entity) {
                        return read_component_property<
                            audio::AudioSource>(
                                world,
                                entity,
                                type,
                                [member](
                                    const audio::AudioSource& value) {
                                    return core::PropertyValue{
                                        value.*member};
                                });
                    },
                    [type, member](
                        core::World& world,
                        core::Entity entity,
                        const core::PropertyValue& value) {
                        return write_component_property<
                            audio::AudioSource>(
                                world,
                                entity,
                                type,
                                value,
                                [member](
                                    audio::AudioSource& source,
                                    const core::PropertyValue& raw) {
                                    const auto* typed =
                                        std::get_if<bool>(
                                            &raw);
                                    if (!typed) return false;
                                    source.*member = *typed;
                                    return true;
                                });
                    }) &&
                ok;
        };

    bool_property(
        "Enabled",
        &audio::AudioSource::enabled);

    bool_property(
        "Play On Awake",
        &audio::AudioSource::play_on_awake);

    bool_property(
        "Loop",
        &audio::AudioSource::loop);

    bool_property(
        "Spatialize",
        &audio::AudioSource::spatialize);

    ok =
        properties.register_property(
            type,
            "Clip",
            core::PropertyKind::AssetReference,
            [type](
                const core::World& world,
                core::Entity entity) {
                return read_component_property<
                    audio::AudioSource>(
                        world,
                        entity,
                        type,
                        [](const audio::AudioSource& source) {
                            return core::PropertyValue{
                                asset_reference_text(
                                    source.clip)};
                        });
            },
            [type](
                core::World& world,
                core::Entity entity,
                const core::PropertyValue& value) {
                return write_component_property<
                    audio::AudioSource>(
                        world,
                        entity,
                        type,
                        value,
                        [](audio::AudioSource& source,
                           const core::PropertyValue& raw) {
                            return parse_asset_reference(
                                raw,
                                source.clip);
                        });
            }) &&
        ok;

    const auto float_property =
        [&properties, type, &ok](
            const char* name,
            float audio::AudioSource::* member,
            float minimum,
            float maximum) {

            ok =
                properties.register_property(
                    type,
                    name,
                    core::PropertyKind::Float,
                    [type, member](
                        const core::World& world,
                        core::Entity entity) {
                        return read_component_property<
                            audio::AudioSource>(
                                world,
                                entity,
                                type,
                                [member](
                                    const audio::AudioSource& value) {
                                    return core::PropertyValue{
                                        static_cast<double>(
                                            value.*member)};
                                });
                    },
                    [type, member, minimum, maximum](
                        core::World& world,
                        core::Entity entity,
                        const core::PropertyValue& value) {
                        return write_component_property<
                            audio::AudioSource>(
                                world,
                                entity,
                                type,
                                value,
                                [member, minimum, maximum](
                                    audio::AudioSource& source,
                                    const core::PropertyValue& raw) {
                                    const auto* typed =
                                        std::get_if<double>(
                                            &raw);
                                    if (!typed ||
                                        *typed < minimum ||
                                        *typed > maximum) {
                                        return false;
                                    }
                                    source.*member =
                                        static_cast<float>(
                                            *typed);
                                    return true;
                                });
                    }) &&
                ok;
        };

    float_property(
        "Volume",
        &audio::AudioSource::volume,
        0.0f,
        1.0f);

    float_property(
        "Pitch",
        &audio::AudioSource::pitch,
        0.0f,
        16.0f);

    float_property(
        "Pan Stereo",
        &audio::AudioSource::pan_stereo,
        -1.0f,
        1.0f);

    return ok;
}

bool register_listener_properties(
    PropertyAccessRegistry& properties) {

    bool ok = true;
    const auto type =
        audio::audio_listener_type();

    ok =
        properties.register_property(
            type,
            "Enabled",
            core::PropertyKind::Boolean,
            [type](
                const core::World& world,
                core::Entity entity) {
                return read_component_property<
                    audio::AudioListener>(
                        world,
                        entity,
                        type,
                        [](const audio::AudioListener& listener) {
                            return core::PropertyValue{
                                listener.enabled};
                        });
            },
            [type](
                core::World& world,
                core::Entity entity,
                const core::PropertyValue& value) {
                return write_component_property<
                    audio::AudioListener>(
                        world,
                        entity,
                        type,
                        value,
                        [](audio::AudioListener& listener,
                           const core::PropertyValue& raw) {
                            const auto* typed =
                                std::get_if<bool>(
                                    &raw);
                            if (!typed) return false;
                            listener.enabled = *typed;
                            return true;
                        });
            }) &&
        ok;

    ok =
        properties.register_property(
            type,
            "Volume",
            core::PropertyKind::Float,
            [type](
                const core::World& world,
                core::Entity entity) {
                return read_component_property<
                    audio::AudioListener>(
                        world,
                        entity,
                        type,
                        [](const audio::AudioListener& listener) {
                            return core::PropertyValue{
                                static_cast<double>(
                                    listener.volume)};
                        });
            },
            [type](
                core::World& world,
                core::Entity entity,
                const core::PropertyValue& value) {
                return write_component_property<
                    audio::AudioListener>(
                        world,
                        entity,
                        type,
                        value,
                        [](audio::AudioListener& listener,
                           const core::PropertyValue& raw) {
                            const auto* typed =
                                std::get_if<double>(
                                    &raw);
                            if (!typed ||
                                *typed < 0.0 ||
                                *typed > 1.0) {
                                return false;
                            }
                            listener.volume =
                                static_cast<float>(
                                    *typed);
                            return true;
                        });
            }) &&
        ok;

    return ok;
}

} // namespace

bool register_audio_integration(
    core::ComponentRegistry& components,
    core::ComponentSerializationRegistry& serialization,
    PropertyAccessRegistry& properties) {

    bool ok = true;

    ok =
        audio::register_component_metadata(
            components) &&
        ok;

    ok =
        audio::register_component_serializers(
            serialization) &&
        ok;

    ok =
        register_source_properties(
            properties) &&
        ok;

    ok =
        register_listener_properties(
            properties) &&
        ok;

    return ok;
}

bool register_audio_component_factories(
    ComponentFactoryRegistry& factories) {

    bool ok = true;

    ok =
        factories.register_factory(
            audio::audio_source_type(),
            [](core::World& world,
               core::Entity entity) {
                return world.add_component<
                    audio::AudioSource>(
                        entity,
                        audio::audio_source_type()) !=
                    nullptr;
            }) &&
        ok;

    ok =
        factories.register_factory(
            audio::audio_listener_type(),
            [](core::World& world,
               core::Entity entity) {
                return world.add_component<
                    audio::AudioListener>(
                        entity,
                        audio::audio_listener_type()) !=
                    nullptr;
            }) &&
        ok;

    return ok;
}

} // namespace nengine::editor
