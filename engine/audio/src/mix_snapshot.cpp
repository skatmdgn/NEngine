#include "nengine/audio/mix_snapshot.hpp"

#include <algorithm>
#include <cmath>

#include "nengine/audio/components.hpp"

namespace nengine::audio {
namespace {

float clamp_unit(float value) noexcept {
    return std::clamp(
        value,
        0.0f,
        1.0f);
}

float clamp_pan(float value) noexcept {
    return std::clamp(
        value,
        -1.0f,
        1.0f);
}

float distance_gain(
    core::Vec3 source,
    core::Vec3 listener) noexcept {

    const float dx =
        source.x - listener.x;
    const float dy =
        source.y - listener.y;
    const float dz =
        source.z - listener.z;

    const float distance =
        std::sqrt(
            dx * dx +
            dy * dy +
            dz * dz);

    return 1.0f /
        (1.0f + distance);
}

float spatial_pan(
    core::Vec3 source,
    core::Vec3 listener) noexcept {

    const float dx =
        source.x - listener.x;
    const float dz =
        source.z - listener.z;

    const float denominator =
        std::abs(dx) +
        std::abs(dz);

    if (denominator <=
        0.000001f) {
        return 0.0f;
    }

    return clamp_pan(
        dx / denominator);
}

void stereo_gains(
    float gain,
    float pan,
    float& left,
    float& right) noexcept {

    const float clamped_pan =
        clamp_pan(pan);

    left =
        gain *
        (clamped_pan <= 0.0f
            ? 1.0f
            : 1.0f - clamped_pan);

    right =
        gain *
        (clamped_pan >= 0.0f
            ? 1.0f
            : 1.0f + clamped_pan);
}

} // namespace

AudioMixSnapshot build_mix_snapshot(
    const core::World& world) {

    AudioMixSnapshot snapshot;

    for (const auto entity :
         world.entities()) {

        if (!world.active(entity)) {
            continue;
        }

        const auto* listener =
            world.get_component<
                AudioListener>(
                    entity,
                    audio_listener_type());

        const auto* transform =
            world.transform(entity);

        if (!listener ||
            !listener->enabled ||
            !transform) {
            continue;
        }

        snapshot.has_listener = true;
        snapshot.listener.entity =
            entity;
        snapshot.listener.position =
            transform->local_position;
        snapshot.listener.volume =
            clamp_unit(
                listener->volume);
        break;
    }

    const core::Vec3 listener_position =
        snapshot.has_listener
            ? snapshot.listener.position
            : core::Vec3{};

    const float listener_volume =
        snapshot.has_listener
            ? snapshot.listener.volume
            : 1.0f;

    for (const auto entity :
         world.entities()) {

        if (!world.active(entity)) {
            continue;
        }

        const auto* source =
            world.get_component<
                AudioSource>(
                    entity,
                    audio_source_type());

        const auto* transform =
            world.transform(entity);

        if (!source ||
            !source->enabled ||
            !source->clip.valid() ||
            !transform) {
            continue;
        }

        float gain =
            clamp_unit(
                source->volume) *
            listener_volume;

        float pan =
            clamp_pan(
                source->pan_stereo);

        if (source->spatialize) {
            gain *=
                distance_gain(
                    transform->local_position,
                    listener_position);

            pan =
                clamp_pan(
                    pan +
                    spatial_pan(
                        transform->local_position,
                        listener_position));
        }

        AudioSourceMixState state;
        state.entity = entity;
        state.clip = source->clip;
        state.position =
            transform->local_position;
        state.spatialized =
            source->spatialize;
        state.playing =
            source->playing;
        state.loop =
            source->loop;
        state.time_seconds =
            std::max(
                0.0f,
                source->time_seconds);
        state.pitch =
            std::max(
                0.0f,
                source->pitch);

        stereo_gains(
            gain,
            pan,
            state.left_gain,
            state.right_gain);

        snapshot.sources.push_back(
            state);
    }

    return snapshot;
}

} // namespace nengine::audio
