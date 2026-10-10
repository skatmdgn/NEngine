#include "nengine/audio/audio_renderer.hpp"

#include <algorithm>
#include <cmath>

namespace nengine::audio {
namespace {

float sample_channel(
    const AudioClipData& clip,
    std::size_t frame,
    std::uint16_t channel) noexcept {

    if (clip.channels == 0 ||
        frame >= clip.frame_count()) {
        return 0.0f;
    }

    const auto resolved_channel =
        static_cast<std::size_t>(
            std::min<std::uint16_t>(
                channel,
                static_cast<std::uint16_t>(
                    clip.channels - 1u)));

    return clip.samples[
        frame *
            static_cast<std::size_t>(
                clip.channels) +
        resolved_channel];
}

float sample_linear(
    const AudioClipData& clip,
    double frame_position,
    std::uint16_t channel,
    bool loop) noexcept {

    const auto frames =
        clip.frame_count();

    if (frames == 0u ||
        frame_position < 0.0) {
        return 0.0f;
    }

    double position =
        frame_position;

    if (loop) {
        position =
            std::fmod(
                position,
                static_cast<double>(
                    frames));

        if (position < 0.0) {
            position +=
                static_cast<double>(
                    frames);
        }
    } else if (
        position >=
        static_cast<double>(frames)) {
        return 0.0f;
    }

    const auto frame0 =
        static_cast<std::size_t>(
            std::floor(position));

    std::size_t frame1 =
        frame0 + 1u;

    if (frame1 >= frames) {
        frame1 =
            loop
                ? 0u
                : frame0;
    }

    const float fraction =
        static_cast<float>(
            position -
            static_cast<double>(
                frame0));

    const float a =
        sample_channel(
            clip,
            frame0,
            channel);

    const float b =
        sample_channel(
            clip,
            frame1,
            channel);

    return a +
        (b - a) *
            fraction;
}

} // namespace

AudioRenderStats render_stereo_mix(
    const AudioMixSnapshot& snapshot,
    const AudioClipResolver& resolver,
    std::uint32_t output_sample_rate,
    std::size_t frame_count,
    std::vector<float>& interleaved_stereo) {

    AudioRenderStats stats;
    stats.frames_rendered =
        frame_count;

    interleaved_stereo.assign(
        frame_count * 2u,
        0.0f);

    if (output_sample_rate == 0u ||
        frame_count == 0u) {
        return stats;
    }

    for (const auto& source :
         snapshot.sources) {

        ++stats.sources_considered;

        if (!source.playing ||
            !source.clip.valid() ||
            source.pitch <= 0.0f) {
            continue;
        }

        const auto* clip =
            resolver
                ? resolver(source.clip)
                : nullptr;

        if (!clip ||
            !clip->valid()) {
            ++stats.unresolved_clips;
            continue;
        }

        ++stats.sources_mixed;

        const double clip_rate =
            static_cast<double>(
                clip->sample_rate);

        const double output_rate =
            static_cast<double>(
                output_sample_rate);

        const double start_frame =
            static_cast<double>(
                std::max(
                    source.time_seconds,
                    0.0f)) *
            clip_rate;

        const double step =
            clip_rate /
            output_rate *
            static_cast<double>(
                source.pitch);

        for (std::size_t frame = 0;
             frame < frame_count;
             ++frame) {

            const double position =
                start_frame +
                static_cast<double>(
                    frame) *
                    step;

            if (!source.loop &&
                position >=
                    static_cast<double>(
                        clip->frame_count())) {
                break;
            }

            float left_sample = 0.0f;
            float right_sample = 0.0f;

            if (clip->channels == 1u) {
                const float mono =
                    sample_linear(
                        *clip,
                        position,
                        0u,
                        source.loop);

                left_sample = mono;
                right_sample = mono;
            } else {
                left_sample =
                    sample_linear(
                        *clip,
                        position,
                        0u,
                        source.loop);

                right_sample =
                    sample_linear(
                        *clip,
                        position,
                        1u,
                        source.loop);
            }

            const auto index =
                frame * 2u;

            interleaved_stereo[index] +=
                left_sample *
                source.left_gain;

            interleaved_stereo[index + 1u] +=
                right_sample *
                source.right_gain;
        }
    }

    for (auto& sample :
         interleaved_stereo) {

        sample =
            std::clamp(
                sample,
                -1.0f,
                1.0f);
    }

    return stats;
}

} // namespace nengine::audio
