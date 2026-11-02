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

// AUD-005: finite 12-tap low-pass reconstruction on downsampling.
// Linear interpolation aliases when source_rate*pitch > output_rate.
// Keep the linear path for 1:1/up-sampling to avoid changing existing
// sample-exact startup behavior.
float sample_resampled(const AudioClipData& clip, double frame_position,
                       std::uint16_t channel, bool loop, double step) noexcept {
    if (!std::isfinite(frame_position) || !std::isfinite(step) || step<=0.0)
        return 0.0f;
    if (step<=1.0 || clip.frame_count()<4u)
        return sample_linear(clip,frame_position,channel,loop);
    const double radius=12.0, cutoff=std::min(1.0,1.0/step);
    constexpr double pi=3.14159265358979323846;
    const auto count=static_cast<double>(clip.frame_count());
    if(count<=0.0)return 0.f;
    // Normalize before converting a potentially long-running playback time
    // to integer sample indices (avoids overflow/undefined conversions).
    if(loop){
        frame_position=std::fmod(frame_position,count);
        if(frame_position<0.0)frame_position+=count;
    }else if(frame_position>=count || frame_position<0.0){
        return 0.0f;
    }
    double weighted=0.0,weight_sum=0.0;
    const double center=std::floor(frame_position);
    for(int k=-11;k<=12;++k){
        const double pos=center+static_cast<double>(k);
        const double offset=frame_position-pos;
        if (std::abs(offset)>=radius)continue;
        const double sinc_argument=pi*cutoff*offset;
        const double sinc=std::abs(sinc_argument)<1e-9?
            1.0:std::sin(sinc_argument)/sinc_argument;
        const double window=0.5+0.5*std::cos(pi*offset/radius);
        const double weight=cutoff*sinc*window;
        auto index=static_cast<std::int64_t>(pos);
        if(loop){
            const auto count=static_cast<std::int64_t>(clip.frame_count());
            index=((index%count)+count)%count;
        }else{
            index=std::clamp<std::int64_t>(
                index,0,static_cast<std::int64_t>(clip.frame_count())-1);
        }
        weighted+=weight*sample_channel(clip,static_cast<std::size_t>(index),channel);
        weight_sum+=weight;
    }
    return std::abs(weight_sum)>1e-9?
        static_cast<float>(weighted/weight_sum):0.0f;
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
            !std::isfinite(source.pitch) || source.pitch <= 0.0f) {
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
                    sample_resampled(
                        *clip,
                        position,
                        0u,
                        source.loop,step);

                left_sample = mono;
                right_sample = mono;
            } else {
                left_sample =
                    sample_resampled(
                        *clip,
                        position,
                        0u,
                        source.loop,step);

                right_sample =
                    sample_resampled(
                        *clip,
                        position,
                        1u,
                        source.loop,step);
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
