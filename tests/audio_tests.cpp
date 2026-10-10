#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>

#include "nengine/audio/components.hpp"
#include "nengine/audio/mix_snapshot.hpp"
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

    core::World mix_world;

    const auto listener_entity =
        mix_world.create(
            "Listener");

    const auto source_entity =
        mix_world.create(
            "Spatial Source");

    auto* mix_listener =
        mix_world.add_component<
            audio::AudioListener>(
                listener_entity,
                audio::audio_listener_type());

    auto* mix_source =
        mix_world.add_component<
            audio::AudioSource>(
                source_entity,
                audio::audio_source_type());

    const auto mix_clip =
        assets::AssetGuid::parse(
            "aaaaaaaaaaaaaaaabbbbbbbbbbbbbbbb");

    if (mix_listener) {
        mix_listener->volume =
            0.5f;
    }

    if (mix_source && mix_clip) {
        mix_source->clip =
            *mix_clip;
        mix_source->spatialize =
            true;
        mix_source->volume =
            1.0f;
        mix_source->playing =
            true;
        mix_source->time_seconds =
            2.0f;
        mix_source->pitch =
            1.25f;
    }

    if (auto* transform =
            mix_world.transform(
                source_entity)) {
        transform->local_position =
            {1.0f, 0.0f, 0.0f};
    }

    const auto mix_snapshot =
        audio::build_mix_snapshot(
            mix_world);

    check(
        mix_snapshot.has_listener &&
        mix_snapshot.listener.entity ==
            listener_entity &&
        mix_snapshot.sources.size() ==
            1u &&
        mix_snapshot.sources.front().entity ==
            source_entity &&
        mix_snapshot.sources.front().playing &&
        mix_snapshot.sources.front().spatialized &&
        std::abs(
            mix_snapshot.sources.front().left_gain) <
            0.0001f &&
        std::abs(
            mix_snapshot.sources.front().right_gain -
            0.25f) < 0.0001f &&
        std::abs(
            mix_snapshot.sources.front().time_seconds -
            2.0f) < 0.0001f &&
        std::abs(
            mix_snapshot.sources.front().pitch -
            1.25f) < 0.0001f,
        "audio mix snapshot selects listener and computes spatial stereo attenuation");

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
