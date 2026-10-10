#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>

#include "nengine/audio/components.hpp"
#include "nengine/audio/registration.hpp"
#include "nengine/core/component_registry.hpp"
#include "nengine/core/component_serialization.hpp"
#include "nengine/core/world.hpp"

namespace {

int failures = 0;

void check(
    bool condition,
    const char* message) {

    if (!condition) {
        ++failures;
        std::cerr
            << "FAIL: "
            << message
            << '\n';
    }
}

void set_property(
    nengine::core::SerializedComponentData& data,
    const char* name,
    nengine::core::PropertyValue value) {

    for (auto& property :
         data.properties) {
        if (property.name == name) {
            property.value =
                std::move(value);
            return;
        }
    }
}

} // namespace

int main() {
    using namespace nengine;

    core::ComponentRegistry metadata;
    core::ComponentSerializationRegistry
        serialization;

    check(
        audio::register_component_metadata(
            metadata),
        "audio component metadata registers");

    check(
        audio::register_component_serializers(
            serialization),
        "audio component serializers register");

    check(
        metadata.find(
            audio::audio_source_type()) != nullptr &&
        metadata.find(
            audio::audio_listener_type()) != nullptr,
        "AudioSource and AudioListener descriptors are discoverable");

    core::World world;
    const auto entity =
        world.create(
            "Audio Emitter");

    auto* source =
        world.add_component<
            audio::AudioSource>(
                entity,
                audio::audio_source_type());

    auto* listener =
        world.add_component<
            audio::AudioListener>(
                entity,
                audio::audio_listener_type());

    check(
        source &&
        listener,
        "audio components attach to World entities");

    const auto clip =
        assets::AssetGuid::parse(
            "11111111111111112222222222222222");

    if (source && clip) {
        source->clip = *clip;
        source->play_on_awake = false;
        source->loop = true;
        source->spatialize = true;
        source->volume = 0.75f;
        source->pitch = 1.25f;
        source->pan_stereo = -0.5f;
        source->playing = true;
        source->time_seconds = 12.0f;
    }

    if (listener) {
        listener->volume = 0.6f;
    }

    const auto captured_source =
        serialization.capture(
            world,
            entity,
            audio::audio_source_type());

    const auto captured_listener =
        serialization.capture(
            world,
            entity,
            audio::audio_listener_type());

    check(
        captured_source &&
        captured_listener,
        "audio codecs capture source and listener data");

    core::World restored;
    const auto restored_entity =
        restored.create(
            "Restored Audio");

    std::string error;

    check(
        captured_source &&
        serialization.restore(
            restored,
            restored_entity,
            *captured_source,
            &error) &&
        captured_listener &&
        serialization.restore(
            restored,
            restored_entity,
            *captured_listener,
            &error),
        "audio codecs restore source and listener data");

    const auto* restored_source =
        restored.get_component<
            audio::AudioSource>(
                restored_entity,
                audio::audio_source_type());

    const auto* restored_listener =
        restored.get_component<
            audio::AudioListener>(
                restored_entity,
                audio::audio_listener_type());

    check(
        restored_source &&
        clip &&
        restored_source->clip == *clip &&
        !restored_source->play_on_awake &&
        restored_source->loop &&
        restored_source->spatialize &&
        std::abs(
            restored_source->volume -
            0.75f) < 0.0001f &&
        std::abs(
            restored_source->pitch -
            1.25f) < 0.0001f &&
        std::abs(
            restored_source->pan_stereo +
            0.5f) < 0.0001f &&
        !restored_source->playing &&
        restored_source->time_seconds == 0.0f &&
        restored_listener &&
        std::abs(
            restored_listener->volume -
            0.6f) < 0.0001f,
        "audio Scene roundtrip preserves serialized values and resets runtime playback state");

    if (captured_source) {
        auto invalid =
            *captured_source;

        set_property(
            invalid,
            "Pan Stereo",
            core::PropertyValue{2.0});

        check(
            !serialization.restore(
                restored,
                restored_entity,
                invalid,
                &error),
            "AudioSource codec rejects pan outside -1..1");
    }

    if (captured_listener) {
        auto invalid =
            *captured_listener;

        set_property(
            invalid,
            "Volume",
            core::PropertyValue{1.5});

        check(
            !serialization.restore(
                restored,
                restored_entity,
                invalid,
                &error),
            "AudioListener codec rejects volume above 1");
    }

    if (failures == 0) {
        std::cout
            << "NEngineAudioTests: all checks passed\n";
        return EXIT_SUCCESS;
    }

    std::cerr
        << "NEngineAudioTests: "
        << failures
        << " failure(s)\n";

    return EXIT_FAILURE;
}
