#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace nengine::audio {

struct AudioClipData {
    std::uint32_t sample_rate{0};
    std::uint16_t channels{0};
    std::vector<float> samples{};

    std::size_t frame_count() const noexcept {
        return channels == 0
            ? 0u
            : samples.size() /
                static_cast<std::size_t>(
                    channels);
    }

    float duration_seconds() const noexcept {
        return sample_rate == 0
            ? 0.0f
            : static_cast<float>(
                frame_count()) /
                static_cast<float>(
                    sample_rate);
    }

    bool valid() const noexcept {
        return sample_rate > 0 &&
               channels > 0 &&
               !samples.empty() &&
               samples.size() %
                   static_cast<std::size_t>(
                       channels) == 0u;
    }
};

bool decode_wav(
    std::span<const std::uint8_t> bytes,
    AudioClipData& output,
    std::string* error = nullptr);

} // namespace nengine::audio
