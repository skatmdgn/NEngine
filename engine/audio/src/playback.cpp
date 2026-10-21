#include "nengine/audio/playback.hpp"

#include <cmath>
#include <limits>
#include <unordered_set>

#include "nengine/audio/components.hpp"

namespace nengine::audio {

AudioPlaybackStats AudioPlaybackSystem::update(
    core::World& world,
    float delta_seconds,
    const AudioClipDurationResolver&
        duration_resolver) {

    AudioPlaybackStats stats;

    if (!std::isfinite(delta_seconds) ||
        delta_seconds < 0.0f) {
        ++stats.unresolved;
        return stats;
    }

    std::unordered_set<
        core::Entity::value_type>
        present;

    for (const auto entity :
         world.entities()) {

        auto* source =
            world.get_component<
                AudioSource>(
                    entity,
                    audio_source_type());

        if (!source) {
            continue;
        }

        present.insert(
            entity.value);

        if (!world.active(entity) ||
            !source->enabled) {
            continue;
        }

        const bool first_update =
            initialized_
                .insert(
                    entity.value)
                .second;

        if (first_update &&
            source->play_on_awake &&
            source->clip.valid()) {

            source->playing = true;
            source->time_seconds = 0.0f;
            ++stats.started;
        }

        if (!source->clip.valid()) {
            if (source->playing) {
                source->playing = false;
                source->time_seconds = 0.0f;
                ++stats.stopped;
            }
            continue;
        }

        if (!std::isfinite(source->time_seconds) ||
            source->time_seconds < 0.0f)
            source->time_seconds = 0.0f;

        if (!std::isfinite(source->pitch)) {
            source->playing = false;
            ++stats.unresolved;
            continue;
        }
        if (!source->playing || source->pitch <= 0.0f ||
            delta_seconds <= 0.0f) continue;

        const double advanced = static_cast<double>(source->time_seconds) +
            static_cast<double>(delta_seconds) * source->pitch;
        if (!std::isfinite(advanced) ||
            advanced > static_cast<double>(std::numeric_limits<float>::max())) {
            source->playing = false;
            source->time_seconds = 0.0f;
            ++stats.unresolved;
            continue;
        }
        source->time_seconds = static_cast<float>(advanced);
        ++stats.advanced;

        std::optional<float>
            duration;

        if (duration_resolver) {
            duration =
                duration_resolver(
                    source->clip);
        }

        if (!duration ||
            !std::isfinite(*duration) ||
            *duration <= 0.0f) {
            if (duration_resolver) {
                ++stats.unresolved;
            }
            continue;
        }

        if (source->time_seconds <
            *duration) {
            continue;
        }

        if (source->loop) {
            source->time_seconds =
                std::fmod(
                    source->time_seconds,
                    *duration);

            ++stats.looped;
        } else {
            source->time_seconds =
                *duration;
            source->playing = false;
            ++stats.stopped;
        }
    }

    for (auto it =
             initialized_.begin();
         it != initialized_.end();) {

        if (present.contains(*it)) {
            ++it;
        } else {
            it =
                initialized_.erase(it);
        }
    }

    return stats;
}

void AudioPlaybackSystem::reset()
    noexcept {

    initialized_.clear();
}

} // namespace nengine::audio
