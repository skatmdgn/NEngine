#include "nengine/audio/audio_device.hpp"

#include <utility>
#include <vector>

namespace nengine::audio {

struct AudioOutputDevice::Impl {
    bool open{false};
    std::uint32_t sample_rate{48000};
    std::uint16_t channels{2};
    std::size_t buffer_frames{1440};
    std::uint64_t submitted_frames{0};
    std::vector<float> scratch{};
};

AudioOutputDevice::AudioOutputDevice()
    : impl_(
          std::make_unique<Impl>()) {}

AudioOutputDevice::~AudioOutputDevice() =
    default;

AudioOutputDevice::AudioOutputDevice(
    AudioOutputDevice&&) noexcept =
    default;

AudioOutputDevice&
AudioOutputDevice::operator=(
    AudioOutputDevice&&) noexcept =
    default;

bool AudioOutputDevice::open(
    std::string* error) {

    impl_->open = true;

    if (error) {
        error->clear();
    }

    return true;
}

void AudioOutputDevice::close() noexcept {
    if (!impl_) return;

    impl_->open = false;
    impl_->submitted_frames = 0;
    impl_->scratch.clear();
}

bool AudioOutputDevice::is_open()
    const noexcept {

    return impl_ &&
        impl_->open;
}

AudioDeviceInfo AudioOutputDevice::info()
    const {

    AudioDeviceInfo result;

    if (!impl_) {
        return result;
    }

    result.open = impl_->open;
    result.backend = "Null";
    result.sample_rate =
        impl_->sample_rate;
    result.channels =
        impl_->channels;
    result.buffer_frames =
        impl_->buffer_frames;
    result.buffer_duration_ms =
        impl_->sample_rate == 0u
            ? 0.0
            : static_cast<double>(
                  impl_->buffer_frames) *
                1000.0 /
                static_cast<double>(
                    impl_->sample_rate);
    result.submitted_frames =
        impl_->submitted_frames;

    return result;
}

AudioRenderStats AudioOutputDevice::pump(
    const AudioMixSnapshot& snapshot,
    const AudioClipResolver& resolver,
    std::string* error) {

    if (!impl_ ||
        !impl_->open) {

        if (error) {
            *error =
                "audio output device is not open";
        }

        return {};
    }

    auto stats =
        render_stereo_mix(
            snapshot,
            resolver,
            impl_->sample_rate,
            impl_->buffer_frames,
            impl_->scratch);

    impl_->submitted_frames +=
        static_cast<std::uint64_t>(
            stats.frames_rendered);

    if (error) {
        error->clear();
    }

    return stats;
}

} // namespace nengine::audio
