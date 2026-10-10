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

core::Quat multiply(
    core::Quat a,
    core::Quat b) noexcept {

    return {
        a.w * b.x +
            a.x * b.w +
            a.y * b.z -
            a.z * b.y,
        a.w * b.y -
            a.x * b.z +
            a.y * b.w +
            a.z * b.x,
        a.w * b.z +
            a.x * b.y -
            a.y * b.x +
            a.z * b.w,
        a.w * b.w -
            a.x * b.x -
            a.y * b.y -
            a.z * b.z
    };
}

core::Vec3 rotate(
    core::Quat q,
    core::Vec3 v) noexcept {

    const core::Quat point{
        v.x,
        v.y,
        v.z,
        0.0f};

    const core::Quat inverse{
        -q.x,
        -q.y,
        -q.z,
        q.w};

    const auto rotated =
        multiply(
            multiply(q, point),
            inverse);

    return {
        rotated.x,
        rotated.y,
        rotated.z
    };
}

struct WorldTransform {
    core::Vec3 position{};
    core::Quat rotation{};
    core::Vec3 scale{
        1.0f,
        1.0f,
        1.0f
    };
};

WorldTransform resolve_world_transform(
    const core::World& world,
    core::Entity entity) noexcept {

    WorldTransform result;

    const auto* local =
        world.transform(entity);

    if (!local) {
        return result;
    }

    result.position =
        local->local_position;
    result.rotation =
        local->local_rotation;
    result.scale =
        local->local_scale;

    core::Entity parent =
        local->parent;

    for (std::size_t depth = 0;
         parent.valid() &&
         depth < 256u;
         ++depth) {

        const auto* ancestor =
            world.transform(parent);

        if (!ancestor) {
            break;
        }

        const core::Vec3 scaled{
            result.position.x *
                ancestor->local_scale.x,
            result.position.y *
                ancestor->local_scale.y,
            result.position.z *
                ancestor->local_scale.z
        };

        const auto rotated =
            rotate(
                ancestor->local_rotation,
                scaled);

        result.position = {
            ancestor->local_position.x +
                rotated.x,
            ancestor->local_position.y +
                rotated.y,
            ancestor->local_position.z +
                rotated.z
        };

        result.rotation =
            multiply(
                ancestor->local_rotation,
                result.rotation);

        result.scale = {
            ancestor->local_scale.x *
                result.scale.x,
            ancestor->local_scale.y *
                result.scale.y,
            ancestor->local_scale.z *
                result.scale.z
        };

        parent =
            ancestor->parent;
    }

    return result;
}

float distance_gain(
    core::Vec3 source,
    core::Vec3 listener,
    float min_distance,
    float max_distance) noexcept {

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

    const float minimum =
        std::max(
            min_distance,
            0.0001f);

    const float maximum =
        std::max(
            max_distance,
            minimum);

    if (distance <= minimum) {
        return 1.0f;
    }

    if (distance >= maximum) {
        return 0.0f;
    }

    if (maximum <= minimum +
        0.000001f) {
        return 0.0f;
    }

    return clamp_unit(
        1.0f -
        (distance - minimum) /
            (maximum - minimum));
}

float spatial_pan(
    core::Vec3 source,
    core::Vec3 listener,
    core::Quat listener_rotation) noexcept {

    const core::Vec3 delta{
        source.x - listener.x,
        source.y - listener.y,
        source.z - listener.z
    };

    const core::Quat inverse{
        -listener_rotation.x,
        -listener_rotation.y,
        -listener_rotation.z,
        listener_rotation.w
    };

    const auto local =
        rotate(
            inverse,
            delta);

    const float denominator =
        std::abs(local.x) +
        std::abs(local.z);

    if (denominator <=
        0.000001f) {
        return 0.0f;
    }

    return clamp_pan(
        local.x / denominator);
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

        if (!listener ||
            !listener->enabled) {
            continue;
        }

        snapshot.has_listener = true;
        snapshot.listener.entity =
            entity;
        const auto listener_transform =
            resolve_world_transform(
                world,
                entity);

        snapshot.listener.position =
            listener_transform.position;
        snapshot.listener.rotation =
            listener_transform.rotation;
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

        if (!source ||
            !source->enabled ||
            !source->clip.valid()) {
            continue;
        }

        const auto world_transform =
            resolve_world_transform(
                world,
                entity);

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
                    world_transform.position,
                    listener_position,
                    source->min_distance,
                    source->max_distance);

            pan =
                clamp_pan(
                    pan +
                    spatial_pan(
                        world_transform.position,
                        listener_position,
                        snapshot.listener.rotation));
        }

        AudioSourceMixState state;
        state.entity = entity;
        state.clip = source->clip;
        state.position =
            world_transform.position;
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
