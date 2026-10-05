#include "nengine/render/decoded_texture.hpp"

#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace nengine::render {
namespace {

void set_error(
    std::string* error,
    std::string message) {

    if (error) {
        *error = std::move(message);
    }
}

std::uint16_t read_u16_le(
    const std::uint8_t* bytes) noexcept {

    return
        static_cast<std::uint16_t>(
            bytes[0]) |
        static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(
                bytes[1]) << 8u);
}

std::uint32_t read_u32_le(
    const std::uint8_t* bytes) noexcept {

    return
        static_cast<std::uint32_t>(
            bytes[0]) |
        (static_cast<std::uint32_t>(
            bytes[1]) << 8u) |
        (static_cast<std::uint32_t>(
            bytes[2]) << 16u) |
        (static_cast<std::uint32_t>(
            bytes[3]) << 24u);
}

std::int32_t read_i32_le(
    const std::uint8_t* bytes) noexcept {

    return static_cast<std::int32_t>(
        read_u32_le(bytes));
}

std::string lowercase(
    std::string value) {

    for (auto& ch : value) {
        ch = static_cast<char>(
            std::tolower(
                static_cast<unsigned char>(
                    ch)));
    }

    return value;
}

bool allocate_rgba(
    std::uint32_t width,
    std::uint32_t height,
    DecodedTextureData& output,
    std::string* error) {

    const auto pixels =
        static_cast<std::uint64_t>(
            width) *
        static_cast<std::uint64_t>(
            height);

    if (width == 0 ||
        height == 0 ||
        pixels >
            (std::numeric_limits<
                std::size_t>::max() / 4u)) {

        set_error(
            error,
            "decoded texture dimensions are invalid or too large");
        return false;
    }

    output.width = width;
    output.height = height;
    output.rgba8.assign(
        static_cast<std::size_t>(
            pixels * 4u),
        0u);

    return true;
}

bool decode_bmp(
    const std::filesystem::path& path,
    DecodedTextureData& output,
    std::string* error) {

    std::ifstream input(
        path,
        std::ios::binary |
            std::ios::ate);

    if (!input) {
        set_error(
            error,
            "could not open BMP texture source");
        return false;
    }

    const auto end =
        input.tellg();

    if (end < 54) {
        set_error(
            error,
            "BMP source is too small");
        return false;
    }

    std::vector<std::uint8_t> bytes(
        static_cast<std::size_t>(
            end));

    input.seekg(0);
    input.read(
        reinterpret_cast<char*>(
            bytes.data()),
        static_cast<std::streamsize>(
            bytes.size()));

    if (!input ||
        bytes[0] != 'B' ||
        bytes[1] != 'M') {

        set_error(
            error,
            "invalid BMP signature");
        return false;
    }

    const auto pixel_offset =
        read_u32_le(
            bytes.data() + 10);

    const auto dib_size =
        read_u32_le(
            bytes.data() + 14);

    if (dib_size < 40u ||
        bytes.size() < 54u) {

        set_error(
            error,
            "unsupported BMP DIB header");
        return false;
    }

    const auto signed_width =
        read_i32_le(
            bytes.data() + 18);

    const auto signed_height =
        read_i32_le(
            bytes.data() + 22);

    const auto planes =
        read_u16_le(
            bytes.data() + 26);

    const auto bits_per_pixel =
        read_u16_le(
            bytes.data() + 28);

    const auto compression =
        read_u32_le(
            bytes.data() + 30);

    if (signed_width <= 0 ||
        signed_height == 0 ||
        planes != 1u ||
        (bits_per_pixel != 24u &&
         bits_per_pixel != 32u) ||
        compression != 0u) {

        set_error(
            error,
            "BMP decoder supports only uncompressed 24/32-bit true-color images");
        return false;
    }

    const auto width =
        static_cast<std::uint32_t>(
            signed_width);

    const auto height =
        static_cast<std::uint32_t>(
            signed_height < 0
                ? -static_cast<std::int64_t>(
                    signed_height)
                : signed_height);

    const auto bytes_per_pixel =
        static_cast<std::uint32_t>(
            bits_per_pixel / 8u);

    const auto row_bytes =
        static_cast<std::uint64_t>(
            width) *
        bytes_per_pixel;

    const auto row_stride =
        (row_bytes + 3u) & ~3ull;

    const auto required =
        static_cast<std::uint64_t>(
            pixel_offset) +
        row_stride *
            static_cast<std::uint64_t>(
                height);

    if (required >
        static_cast<std::uint64_t>(
            bytes.size())) {

        set_error(
            error,
            "BMP pixel payload is truncated");
        return false;
    }

    DecodedTextureData decoded;

    if (!allocate_rgba(
            width,
            height,
            decoded,
            error)) {
        return false;
    }

    const bool top_down =
        signed_height < 0;

    for (std::uint32_t y = 0;
         y < height;
         ++y) {

        const auto source_y =
            top_down
                ? y
                : height - 1u - y;

        const auto* row =
            bytes.data() +
            pixel_offset +
            static_cast<std::size_t>(
                row_stride *
                source_y);

        for (std::uint32_t x = 0;
             x < width;
             ++x) {

            const auto* pixel =
                row +
                static_cast<std::size_t>(
                    x *
                    bytes_per_pixel);

            const auto destination =
                (static_cast<std::size_t>(y) *
                    width +
                 x) *
                4u;

            decoded.rgba8[destination + 0] =
                pixel[2];
            decoded.rgba8[destination + 1] =
                pixel[1];
            decoded.rgba8[destination + 2] =
                pixel[0];
            decoded.rgba8[destination + 3] =
                bits_per_pixel == 32u
                    ? pixel[3]
                    : 255u;
        }
    }

    output = std::move(decoded);
    return true;
}

bool decode_tga(
    const std::filesystem::path& path,
    DecodedTextureData& output,
    std::string* error) {

    std::ifstream input(
        path,
        std::ios::binary |
            std::ios::ate);

    if (!input) {
        set_error(
            error,
            "could not open TGA texture source");
        return false;
    }

    const auto end =
        input.tellg();

    if (end < 18) {
        set_error(
            error,
            "TGA source is too small");
        return false;
    }

    std::vector<std::uint8_t> bytes(
        static_cast<std::size_t>(
            end));

    input.seekg(0);
    input.read(
        reinterpret_cast<char*>(
            bytes.data()),
        static_cast<std::streamsize>(
            bytes.size()));

    if (!input) {
        set_error(
            error,
            "could not read TGA texture source");
        return false;
    }

    const auto id_length =
        bytes[0];

    const auto color_map_type =
        bytes[1];

    const auto image_type =
        bytes[2];

    const auto width =
        static_cast<std::uint32_t>(
            read_u16_le(
                bytes.data() + 12));

    const auto height =
        static_cast<std::uint32_t>(
            read_u16_le(
                bytes.data() + 14));

    const auto bits_per_pixel =
        bytes[16];

    const auto descriptor =
        bytes[17];

    if (color_map_type != 0u ||
        image_type != 2u ||
        (bits_per_pixel != 24u &&
         bits_per_pixel != 32u)) {

        set_error(
            error,
            "TGA decoder supports only uncompressed 24/32-bit true-color images");
        return false;
    }

    const auto bytes_per_pixel =
        static_cast<std::uint32_t>(
            bits_per_pixel / 8u);

    const auto pixel_offset =
        18u +
        static_cast<std::uint32_t>(
            id_length);

    const auto payload =
        static_cast<std::uint64_t>(
            width) *
        static_cast<std::uint64_t>(
            height) *
        bytes_per_pixel;

    if (width == 0 ||
        height == 0 ||
        static_cast<std::uint64_t>(
            pixel_offset) +
            payload >
            static_cast<std::uint64_t>(
                bytes.size())) {

        set_error(
            error,
            "TGA pixel payload is invalid or truncated");
        return false;
    }

    DecodedTextureData decoded;

    if (!allocate_rgba(
            width,
            height,
            decoded,
            error)) {
        return false;
    }

    const bool top_origin =
        (descriptor & 0x20u) != 0u;

    const bool right_origin =
        (descriptor & 0x10u) != 0u;

    const auto* pixels =
        bytes.data() +
        pixel_offset;

    for (std::uint32_t source_y = 0;
         source_y < height;
         ++source_y) {

        for (std::uint32_t source_x = 0;
             source_x < width;
             ++source_x) {

            const auto* pixel =
                pixels +
                (static_cast<std::size_t>(
                    source_y) *
                    width +
                 source_x) *
                    bytes_per_pixel;

            const auto x =
                right_origin
                    ? width - 1u -
                        source_x
                    : source_x;

            const auto y =
                top_origin
                    ? source_y
                    : height - 1u -
                        source_y;

            const auto destination =
                (static_cast<std::size_t>(y) *
                    width +
                 x) *
                4u;

            decoded.rgba8[destination + 0] =
                pixel[2];
            decoded.rgba8[destination + 1] =
                pixel[1];
            decoded.rgba8[destination + 2] =
                pixel[0];
            decoded.rgba8[destination + 3] =
                bits_per_pixel == 32u
                    ? pixel[3]
                    : 255u;
        }
    }

    output = std::move(decoded);
    return true;
}

} // namespace

bool decode_texture_rgba8(
    const ResolvedTextureAsset& asset,
    DecodedTextureData& output,
    std::string* error) {

    output = {};

    if (!asset.guid.valid()) {
        set_error(
            error,
            "texture AssetGuid is invalid");
        return false;
    }

    const auto format =
        lowercase(
            asset.metadata.format);

    bool decoded = false;

    if (format == "bmp") {
        decoded =
            decode_bmp(
                asset.source_path,
                output,
                error);
    } else if (
        format == "tga") {

        decoded =
            decode_tga(
                asset.source_path,
                output,
                error);
    } else {
        set_error(
            error,
            "texture pixel decoder is not implemented for format: " +
                format);
        return false;
    }

    if (!decoded ||
        !output.valid()) {
        return false;
    }

    if ((asset.metadata.width != 0 &&
         asset.metadata.width !=
            output.width) ||
        (asset.metadata.height != 0 &&
         asset.metadata.height !=
            output.height)) {

        output = {};
        set_error(
            error,
            "decoded texture dimensions do not match imported metadata");
        return false;
    }

    output.color_space =
        lowercase(
            asset.metadata.color_space) ==
            "linear"
            ? DecodedTextureColorSpace::Linear
            : DecodedTextureColorSpace::SRgb;

    return true;
}

const DecodedTextureData*
DecodedTextureCache::load(
    assets::AssetGuid guid,
    const assets::CachedArtifactSet& artifacts,
    std::string* error) {

    if (!guid.valid()) {
        set_error(
            error,
            "texture cache requires a valid AssetGuid");
        return nullptr;
    }

    const auto existing =
        entries_.find(guid);

    if (existing !=
            entries_.end() &&
        existing->second.fingerprint ==
            artifacts.fingerprint &&
        existing->second.texture.valid()) {

        return &existing->second.texture;
    }

    if (existing !=
        entries_.end()) {
        entries_.erase(existing);
    }

    const auto resolved =
        resolve_texture_asset(
            guid,
            artifacts,
            error);

    if (!resolved) {
        return nullptr;
    }

    DecodedTextureData decoded;

    if (!decode_texture_rgba8(
            *resolved,
            decoded,
            error)) {
        return nullptr;
    }

    Entry entry;
    entry.fingerprint =
        artifacts.fingerprint;
    entry.texture =
        std::move(decoded);

    const auto [it, inserted] =
        entries_.emplace(
            guid,
            std::move(entry));

    if (!inserted) {
        set_error(
            error,
            "decoded texture cache insertion failed");
        return nullptr;
    }

    return &it->second.texture;
}

const DecodedTextureData*
DecodedTextureCache::find(
    assets::AssetGuid guid) const noexcept {

    const auto it =
        entries_.find(guid);

    return it ==
        entries_.end()
        ? nullptr
        : &it->second.texture;
}

bool DecodedTextureCache::erase(
    assets::AssetGuid guid) noexcept {

    return entries_.erase(guid) != 0u;
}

void DecodedTextureCache::clear() noexcept {
    entries_.clear();
}

} // namespace nengine::render
