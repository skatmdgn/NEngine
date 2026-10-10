#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "nengine/audio/audio_renderer.hpp"

namespace nengine::audio {

struct AudioDeviceInfo {
    bool open{false};
    std::string backend{};
    std::string diagnostic{};
    std::uint32_t sample_rate{0};
    std::uint16_t channels{0};
    std::size_t buffer_frames{0};
    double buffer_duration_ms{0.0};
    std::uint64_t submitted_frames{0};
};

class AudioOutputDevice {
public:
    AudioOutputDevice();
    ~AudioOutputDevice();

    AudioOutputDevice(
        const AudioOutputDevice&) = delete;
    AudioOutputDevice& operator=(
        const AudioOutputDevice&) = delete;

    AudioOutputDevice(
        AudioOutputDevice&&) noexcept;
    AudioOutputDevice& operator=(
        AudioOutputDevice&&) noexcept;

    bool open(
        std::string* error = nullptr);

    void close() noexcept;

    bool is_open() const noexcept;

    AudioDeviceInfo info() const;

    AudioRenderStats pump(
        const AudioMixSnapshot& snapshot,
        const AudioClipResolver& resolver,
        std::string* error = nullptr);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace nengine::audio
