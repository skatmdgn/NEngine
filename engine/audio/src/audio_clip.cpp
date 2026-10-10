#include "nengine/audio/audio_clip.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>

namespace nengine::audio {
namespace {

void set_error(
    std::string* error,
    const char* message) {

    if (error) {
        *error = message;
    }
}

bool tag_equals(
    std::span<const std::uint8_t> bytes,
    std::size_t offset,
    const char (&tag)[5]) noexcept {

    if (offset + 4u >
        bytes.size()) {
        return false;
    }

    return bytes[offset + 0u] ==
               static_cast<std::uint8_t>(
                   tag[0]) &&
           bytes[offset + 1u] ==
               static_cast<std::uint8_t>(
                   tag[1]) &&
           bytes[offset + 2u] ==
               static_cast<std::uint8_t>(
                   tag[2]) &&
           bytes[offset + 3u] ==
               static_cast<std::uint8_t>(
                   tag[3]);
}

bool read_u16(
    std::span<const std::uint8_t> bytes,
    std::size_t offset,
    std::uint16_t& value) noexcept {

    if (offset + 2u >
        bytes.size()) {
        return false;
    }

    value =
        static_cast<std::uint16_t>(
            bytes[offset]) |
        static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(
                bytes[offset + 1u]) << 8u);

    return true;
}

bool read_u32(
    std::span<const std::uint8_t> bytes,
    std::size_t offset,
    std::uint32_t& value) noexcept {

    if (offset + 4u >
        bytes.size()) {
        return false;
    }

    value =
        static_cast<std::uint32_t>(
            bytes[offset]) |
        (static_cast<std::uint32_t>(
             bytes[offset + 1u]) << 8u) |
        (static_cast<std::uint32_t>(
             bytes[offset + 2u]) << 16u) |
        (static_cast<std::uint32_t>(
             bytes[offset + 3u]) << 24u);

    return true;
}

std::int32_t read_i24(
    const std::uint8_t* sample) noexcept {

    std::int32_t value =
        static_cast<std::int32_t>(
            sample[0]) |
        (static_cast<std::int32_t>(
             sample[1]) << 8) |
        (static_cast<std::int32_t>(
             sample[2]) << 16);

    if ((value & 0x00800000) != 0) {
        value |=
            static_cast<std::int32_t>(
                0xff000000);
    }

    return value;
}

float clamp_sample(float value) noexcept {
    return std::clamp(
        value,
        -1.0f,
        1.0f);
}

} // namespace

bool decode_wav(
    std::span<const std::uint8_t> bytes,
    AudioClipData& output,
    std::string* error) {

    output = {};

    if (bytes.size() < 12u ||
        !tag_equals(
            bytes,
            0u,
            "RIFF") ||
        !tag_equals(
            bytes,
            8u,
            "WAVE")) {

        set_error(
            error,
            "audio data is not a RIFF/WAVE stream");
        return false;
    }

    std::uint16_t format = 0;
    std::uint16_t channels = 0;
    std::uint32_t sample_rate = 0;
    std::uint16_t block_align = 0;
    std::uint16_t bits_per_sample = 0;

    std::span<const std::uint8_t>
        sample_bytes{};

    bool saw_fmt = false;
    bool saw_data = false;

    std::size_t offset = 12u;

    while (offset + 8u <=
           bytes.size()) {

        std::uint32_t chunk_size = 0;

        if (!read_u32(
                bytes,
                offset + 4u,
                chunk_size)) {
            break;
        }

        const std::size_t data_offset =
            offset + 8u;

        const std::size_t data_end =
            data_offset +
            static_cast<std::size_t>(
                chunk_size);

        if (data_end <
                data_offset ||
            data_end >
                bytes.size()) {

            set_error(
                error,
                "WAV chunk exceeds input size");
            return false;
        }

        if (tag_equals(
                bytes,
                offset,
                "fmt ")) {

            if (chunk_size < 16u ||
                !read_u16(
                    bytes,
                    data_offset + 0u,
                    format) ||
                !read_u16(
                    bytes,
                    data_offset + 2u,
                    channels) ||
                !read_u32(
                    bytes,
                    data_offset + 4u,
                    sample_rate) ||
                !read_u16(
                    bytes,
                    data_offset + 12u,
                    block_align) ||
                !read_u16(
                    bytes,
                    data_offset + 14u,
                    bits_per_sample)) {

                set_error(
                    error,
                    "WAV fmt chunk is malformed");
                return false;
            }

            saw_fmt = true;
        } else if (tag_equals(
                       bytes,
                       offset,
                       "data")) {

            sample_bytes =
                bytes.subspan(
                    data_offset,
                    static_cast<std::size_t>(
                        chunk_size));

            saw_data = true;
        }

        const std::size_t padded =
            static_cast<std::size_t>(
                chunk_size) +
            static_cast<std::size_t>(
                chunk_size & 1u);

        if (data_offset + padded <
            data_offset) {

            set_error(
                error,
                "WAV chunk size overflow");
            return false;
        }

        offset =
            data_offset +
            padded;
    }

    if (!saw_fmt ||
        !saw_data) {

        set_error(
            error,
            "WAV stream requires fmt and data chunks");
        return false;
    }

    if (channels == 0 ||
        channels > 32 ||
        sample_rate == 0) {

        set_error(
            error,
            "WAV channel count or sample rate is invalid");
        return false;
    }

    const bool pcm =
        format == 1u;

    const bool ieee_float =
        format == 3u;

    if (!pcm &&
        !ieee_float) {

        set_error(
            error,
            "WAV codec is unsupported");
        return false;
    }

    if (ieee_float &&
        bits_per_sample != 32u) {

        set_error(
            error,
            "only 32-bit IEEE float WAV is supported");
        return false;
    }

    if (pcm &&
        bits_per_sample != 8u &&
        bits_per_sample != 16u &&
        bits_per_sample != 24u &&
        bits_per_sample != 32u) {

        set_error(
            error,
            "unsupported PCM WAV bit depth");
        return false;
    }

    const std::size_t bytes_per_sample =
        static_cast<std::size_t>(
            bits_per_sample / 8u);

    const std::size_t expected_align =
        bytes_per_sample *
        static_cast<std::size_t>(
            channels);

    if (bytes_per_sample == 0u ||
        block_align != expected_align ||
        sample_bytes.size() %
            block_align != 0u) {

        set_error(
            error,
            "WAV block alignment is invalid");
        return false;
    }

    const std::size_t sample_count =
        sample_bytes.size() /
        bytes_per_sample;

    output.sample_rate =
        sample_rate;

    output.channels =
        channels;

    output.samples.resize(
        sample_count);

    for (std::size_t index = 0;
         index < sample_count;
         ++index) {

        const auto* sample =
            sample_bytes.data() +
            index *
                bytes_per_sample;

        float value = 0.0f;

        if (ieee_float) {
            std::uint32_t raw =
                static_cast<std::uint32_t>(
                    sample[0]) |
                (static_cast<std::uint32_t>(
                     sample[1]) << 8u) |
                (static_cast<std::uint32_t>(
                     sample[2]) << 16u) |
                (static_cast<std::uint32_t>(
                     sample[3]) << 24u);

            value =
                std::bit_cast<float>(
                    raw);

            if (!std::isfinite(value)) {
                set_error(
                    error,
                    "WAV contains a non-finite float sample");
                output = {};
                return false;
            }
        } else {
            switch (bits_per_sample) {
            case 8u:
                value =
                    (static_cast<float>(
                         sample[0]) -
                     128.0f) /
                    128.0f;
                break;

            case 16u: {
                const std::uint16_t raw =
                    static_cast<std::uint16_t>(
                        sample[0]) |
                    static_cast<std::uint16_t>(
                        static_cast<std::uint16_t>(
                            sample[1]) << 8u);

                const auto signed_value =
                    static_cast<std::int16_t>(
                        raw);

                value =
                    static_cast<float>(
                        signed_value) /
                    32768.0f;
                break;
            }

            case 24u:
                value =
                    static_cast<float>(
                        read_i24(sample)) /
                    8388608.0f;
                break;

            case 32u: {
                const std::uint32_t raw =
                    static_cast<std::uint32_t>(
                        sample[0]) |
                    (static_cast<std::uint32_t>(
                         sample[1]) << 8u) |
                    (static_cast<std::uint32_t>(
                         sample[2]) << 16u) |
                    (static_cast<std::uint32_t>(
                         sample[3]) << 24u);

                const auto signed_value =
                    static_cast<std::int32_t>(
                        raw);

                value =
                    static_cast<float>(
                        static_cast<double>(
                            signed_value) /
                        2147483648.0);
                break;
            }

            default:
                break;
            }
        }

        output.samples[index] =
            clamp_sample(
                value);
    }

    if (!output.valid()) {
        set_error(
            error,
            "decoded WAV contains no complete audio frames");
        output = {};
        return false;
    }

    if (error) {
        error->clear();
    }

    return true;
}

} // namespace nengine::audio
