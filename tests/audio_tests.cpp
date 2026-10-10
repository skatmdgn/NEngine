#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "nengine/audio/audio_clip.hpp"
#include "nengine/audio/audio_device.hpp"
#include "nengine/audio/audio_renderer.hpp"
#include "nengine/audio/clip_cache.hpp"
#include "nengine/audio/components.hpp"
#include "nengine/audio/mix_snapshot.hpp"
#include "nengine/audio/playback.hpp"
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

void append_u16(
    std::vector<std::uint8_t>& bytes,
    std::uint16_t value) {

    bytes.push_back(
        static_cast<std::uint8_t>(
            value & 0xffu));

    bytes.push_back(
        static_cast<std::uint8_t>(
            (value >> 8u) & 0xffu));
}

void append_u32(
    std::vector<std::uint8_t>& bytes,
    std::uint32_t value) {

    bytes.push_back(
        static_cast<std::uint8_t>(
            value & 0xffu));

    bytes.push_back(
        static_cast<std::uint8_t>(
            (value >> 8u) & 0xffu));

    bytes.push_back(
        static_cast<std::uint8_t>(
            (value >> 16u) & 0xffu));

    bytes.push_back(
        static_cast<std::uint8_t>(
            (value >> 24u) & 0xffu));
}

void append_tag(
    std::vector<std::uint8_t>& bytes,
    const char (&tag)[5]) {

    for (int index = 0;
         index < 4;
         ++index) {
        bytes.push_back(
            static_cast<std::uint8_t>(
                tag[index]));
    }
}

std::vector<std::uint8_t>
make_pcm16_wav() {

    std::vector<std::uint8_t> bytes;

    append_tag(bytes, "RIFF");
    append_u32(bytes, 44u);
    append_tag(bytes, "WAVE");

    append_tag(bytes, "fmt ");
    append_u32(bytes, 16u);
    append_u16(bytes, 1u);
    append_u16(bytes, 2u);
    append_u32(bytes, 2u);
    append_u32(bytes, 8u);
    append_u16(bytes, 4u);
    append_u16(bytes, 16u);

    append_tag(bytes, "data");
    append_u32(bytes, 8u);

    append_u16(bytes, 0x8000u);
    append_u16(bytes, 0x0000u);
    append_u16(bytes, 0x4000u);
    append_u16(bytes, 0x7fffu);

    return bytes;
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

    const auto source_parent =
        mix_world.create(
            "Spatial Parent");

    const auto source_entity =
        mix_world.create(
            "Spatial Source");

    mix_world.set_parent(
        source_entity,
        source_parent);

    if (auto* transform =
            mix_world.transform(
                source_parent)) {
        transform->local_position =
            {1.0f, 0.0f, 0.0f};
    }

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
            {0.0f, 0.0f, 0.0f};
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
        "audio mix snapshot resolves hierarchy world positions and computes spatial stereo attenuation");

    if (auto* transform =
            mix_world.transform(
                listener_entity)) {
        transform->local_rotation = {
            0.0f,
            0.70710678f,
            0.0f,
            0.70710678f
        };
    }

    const auto rotated_listener_mix =
        audio::build_mix_snapshot(
            mix_world);

    check(
        rotated_listener_mix.sources.size() ==
            1u &&
        std::abs(
            rotated_listener_mix.sources.front().left_gain -
            0.25f) < 0.0002f &&
        std::abs(
            rotated_listener_mix.sources.front().right_gain -
            0.25f) < 0.0002f,
        "spatial stereo pan follows listener world rotation");

    const auto wav_bytes =
        make_pcm16_wav();

    audio::AudioClipData
        decoded_clip;

    std::string wav_error;

    check(
        audio::decode_wav(
            wav_bytes,
            decoded_clip,
            &wav_error) &&
        decoded_clip.valid() &&
        decoded_clip.sample_rate == 2u &&
        decoded_clip.channels == 2u &&
        decoded_clip.frame_count() == 2u &&
        std::abs(
            decoded_clip.duration_seconds() -
            1.0f) < 0.0001f &&
        decoded_clip.samples.size() == 4u &&
        std::abs(
            decoded_clip.samples[0] +
            1.0f) < 0.0001f &&
        std::abs(
            decoded_clip.samples[2] -
            0.5f) < 0.0001f,
        "WAV decoder normalizes interleaved PCM16 samples and exposes clip metadata");

    const std::vector<std::uint8_t>
        malformed_wav{
            'R', 'I', 'F', 'F',
            0, 0, 0, 0,
            'N', 'O', 'P', 'E'
        };

    check(
        !audio::decode_wav(
            malformed_wav,
            decoded_clip,
            &wav_error) &&
        !wav_error.empty(),
        "WAV decoder rejects malformed non-WAVE data with diagnostics");

    core::World playback_world;

    const auto playback_entity =
        playback_world.create(
            "Playback Source");

    auto* playback_source =
        playback_world.add_component<
            audio::AudioSource>(
                playback_entity,
                audio::audio_source_type());

    if (playback_source &&
        mix_clip) {
        playback_source->clip =
            *mix_clip;
        playback_source->play_on_awake =
            true;
        playback_source->pitch =
            2.0f;
        playback_source->loop =
            false;
    }

    audio::AudioPlaybackSystem
        playback_system;

    const auto duration_resolver =
        [](assets::AssetGuid)
            -> std::optional<float> {
            return 1.0f;
        };

    const auto playback_first =
        playback_system.update(
            playback_world,
            0.25f,
            duration_resolver);

    playback_source =
        playback_world.get_component<
            audio::AudioSource>(
                playback_entity,
                audio::audio_source_type());

    check(
        playback_source &&
        playback_first.started == 1u &&
        playback_first.advanced == 1u &&
        playback_source->playing &&
        std::abs(
            playback_source->time_seconds -
            0.5f) < 0.0001f,
        "audio playback starts play-on-awake sources and advances time by pitch");

    const auto playback_second =
        playback_system.update(
            playback_world,
            0.25f,
            duration_resolver);

    check(
        playback_source &&
        playback_second.stopped == 1u &&
        !playback_source->playing &&
        std::abs(
            playback_source->time_seconds -
            1.0f) < 0.0001f,
        "audio playback stops non-looping sources at clip duration");

    if (playback_source) {
        playback_source->loop = true;
        playback_source->playing = true;
        playback_source->pitch = 1.0f;
        playback_source->time_seconds =
            0.75f;
    }

    const auto playback_loop =
        playback_system.update(
            playback_world,
            0.5f,
            duration_resolver);

    check(
        playback_source &&
        playback_loop.looped == 1u &&
        playback_source->playing &&
        std::abs(
            playback_source->time_seconds -
            0.25f) < 0.0001f,
        "audio playback wraps looping sources at clip duration");

    playback_system.reset();

    check(
        playback_system
            .initialized_source_count() ==
            0u,
        "audio playback reset clears play-on-awake state");

    const auto cached_wav_path =
        std::filesystem::temp_directory_path() /
        "nengine_audio_clip_cache_test.wav";

    {
        std::ofstream output(
            cached_wav_path,
            std::ios::binary |
                std::ios::trunc);

        output.write(
            reinterpret_cast<const char*>(
                wav_bytes.data()),
            static_cast<std::streamsize>(
                wav_bytes.size()));
    }

    assets::CachedArtifactSet
        cached_audio;

    cached_audio.fingerprint =
        "audio-fixture-v1";

    cached_audio.importer_id =
        "NEngine.Audio";

    cached_audio.importer_version =
        1u;

    cached_audio.artifacts.push_back({
        cached_wav_path,
        "source"
    });

    audio::AudioClipCache
        clip_cache;

    std::string cache_error;

    const auto* cached_clip =
        mix_clip
            ? clip_cache.load(
                *mix_clip,
                cached_audio,
                &cache_error)
            : nullptr;

    const auto* cached_again =
        mix_clip
            ? clip_cache.load(
                *mix_clip,
                cached_audio,
                &cache_error)
            : nullptr;

    check(
        cached_clip &&
        cached_again ==
            cached_clip &&
        cached_clip->valid() &&
        std::abs(
            cached_clip->duration_seconds() -
            1.0f) < 0.0001f &&
        mix_clip &&
        clip_cache.find(
            *mix_clip) ==
            cached_clip,
        "AudioClipCache decodes staged WAV source artifacts and reuses matching fingerprints");

    clip_cache.clear();

    check(
        !mix_clip ||
        clip_cache.find(
            *mix_clip) == nullptr,
        "AudioClipCache clear invalidates decoded clip entries");

    std::error_code remove_error;
    std::filesystem::remove(
        cached_wav_path,
        remove_error);

    audio::AudioClipData render_clip;
    render_clip.sample_rate = 4u;
    render_clip.channels = 1u;
    render_clip.samples = {
        0.0f, 1.0f, 0.0f, -1.0f
    };

    audio::AudioMixSnapshot render_snapshot;
    audio::AudioSourceMixState render_source;
    if (mix_clip) {
        render_source.clip = *mix_clip;
    }
    render_source.playing = true;
    render_source.loop = true;
    render_source.pitch = 1.0f;
    render_source.left_gain = 0.5f;
    render_source.right_gain = 1.0f;
    render_snapshot.sources.push_back(
        render_source);

    std::vector<float> rendered_audio;
    const auto render_stats =
        audio::render_stereo_mix(
            render_snapshot,
            [&render_clip](assets::AssetGuid) {
                return &render_clip;
            },
            4u,
            4u,
            rendered_audio);

    check(
        render_stats.sources_mixed == 1u &&
        rendered_audio.size() == 8u &&
        std::abs(rendered_audio[2] - 0.5f) < 0.0001f &&
        std::abs(rendered_audio[3] - 1.0f) < 0.0001f &&
        std::abs(rendered_audio[6] + 0.5f) < 0.0001f &&
        std::abs(rendered_audio[7] + 1.0f) < 0.0001f,
        "software audio renderer mixes mono PCM into stereo gains");

    audio::AudioOutputDevice output_device;
    std::string device_error;

    check(
        output_device.open(&device_error) &&
        output_device.is_open(),
        "audio output device abstraction opens");

    const auto output_info =
        output_device.info();

    check(
        output_info.open &&
        output_info.sample_rate > 0u &&
        output_info.channels == 2u &&
        output_info.buffer_frames > 0u,
        "audio output device reports usable stereo format");

    const auto device_stats =
        output_device.pump(
            render_snapshot,
            [&render_clip](assets::AssetGuid) {
                return &render_clip;
            },
            &device_error);

    check(
        device_stats.frames_rendered ==
            output_info.buffer_frames &&
        output_device.info().submitted_frames ==
            output_info.buffer_frames,
        "audio output device pumps mixed PCM and tracks submitted frames");

    output_device.close();

    check(
        !output_device.is_open(),
        "audio output device closes cleanly");

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
