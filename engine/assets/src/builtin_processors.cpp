#include "nengine/assets/builtin_processors.hpp"
#include "nengine/assets/gltf_sidecars.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace nengine::assets {
namespace {

std::string lowercase(
    std::string value) {

    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char ch) {
            return static_cast<char>(
                std::tolower(ch));
        });

    return value;
}

bool stage_source(
    const ImportContext& context,
    ImportResult& result,
    std::filesystem::path& destination) {

    if (!context.asset ||
        !context.importer) {

        result.message =
            "invalid import context";
        return false;
    }

    const auto extension =
        context.asset
            ->source_path
            .extension();

    destination =
        context.cache_directory /
        (std::string{"source"} +
         extension.string());

    std::error_code error;

    std::filesystem::copy_file(
        context.asset->source_path,
        destination,
        std::filesystem::
            copy_options::
                overwrite_existing,
        error);

    if (error) {
        result.message =
            "source copy failed: " +
            error.message();
        return false;
    }

    result.artifacts.push_back({
        destination,
        "source"
    });

    return true;
}

bool write_descriptor(
    const std::filesystem::path& path,
    std::string_view text,
    ImportResult& result,
    std::string role) {

    std::ofstream output(
        path,
        std::ios::binary |
            std::ios::trunc);

    if (!output) {
        result.message =
            "could not create import descriptor";
        return false;
    }

    output.write(
        text.data(),
        static_cast<std::streamsize>(
            text.size()));

    if (!output.good()) {
        result.message =
            "failed writing import descriptor";
        return false;
    }

    result.artifacts.push_back({
        path,
        std::move(role)
    });

    return true;
}

std::uint16_t read_u16_le(
    const unsigned char* bytes) noexcept {

    return
        static_cast<std::uint16_t>(
            bytes[0]) |
        static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(
                bytes[1]) << 8u);
}

std::uint32_t read_u32_le(
    const unsigned char* bytes) noexcept {

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

std::uint32_t read_u32_be(
    const unsigned char* bytes) noexcept {

    return
        (static_cast<std::uint32_t>(
            bytes[0]) << 24u) |
        (static_cast<std::uint32_t>(
            bytes[1]) << 16u) |
        (static_cast<std::uint32_t>(
            bytes[2]) << 8u) |
        static_cast<std::uint32_t>(
            bytes[3]);
}

struct TextureMetadata {
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::string format{"unknown"};
};

bool read_png_metadata(
    const std::filesystem::path& path,
    TextureMetadata& metadata) {

    std::ifstream input(
        path,
        std::ios::binary);

    std::array<unsigned char, 24>
        header{};

    if (!input.read(
            reinterpret_cast<char*>(
                header.data()),
            static_cast<std::streamsize>(
                header.size()))) {
        return false;
    }

    constexpr std::array<
        unsigned char,
        8> signature{
            0x89, 'P', 'N', 'G',
            0x0D, 0x0A, 0x1A, 0x0A
        };

    if (!std::equal(
            signature.begin(),
            signature.end(),
            header.begin())) {
        return false;
    }

    if (!(header[12] == 'I' &&
          header[13] == 'H' &&
          header[14] == 'D' &&
          header[15] == 'R')) {
        return false;
    }

    metadata.width =
        read_u32_be(
            header.data() + 16);

    metadata.height =
        read_u32_be(
            header.data() + 20);

    metadata.format = "png";
    return true;
}

bool read_bmp_metadata(
    const std::filesystem::path& path,
    TextureMetadata& metadata) {

    std::ifstream input(
        path,
        std::ios::binary);

    std::array<unsigned char, 26>
        header{};

    if (!input.read(
            reinterpret_cast<char*>(
                header.data()),
            static_cast<std::streamsize>(
                header.size()))) {
        return false;
    }

    if (header[0] != 'B' ||
        header[1] != 'M') {
        return false;
    }

    metadata.width =
        read_u32_le(
            header.data() + 18);

    metadata.height =
        read_u32_le(
            header.data() + 22);

    metadata.format = "bmp";
    return true;
}

bool read_tga_metadata(
    const std::filesystem::path& path,
    TextureMetadata& metadata) {

    std::ifstream input(
        path,
        std::ios::binary);

    std::array<unsigned char, 18>
        header{};

    if (!input.read(
            reinterpret_cast<char*>(
                header.data()),
            static_cast<std::streamsize>(
                header.size()))) {
        return false;
    }

    metadata.width =
        read_u16_le(
            header.data() + 12);

    metadata.height =
        read_u16_le(
            header.data() + 14);

    metadata.format = "tga";

    return
        metadata.width != 0 &&
        metadata.height != 0;
}

bool is_jpeg_sof_marker(
    unsigned char marker) noexcept {

    switch (marker) {
    case 0xC0:
    case 0xC1:
    case 0xC2:
    case 0xC3:
    case 0xC5:
    case 0xC6:
    case 0xC7:
    case 0xC9:
    case 0xCA:
    case 0xCB:
    case 0xCD:
    case 0xCE:
    case 0xCF:
        return true;
    default:
        return false;
    }
}

bool read_jpeg_metadata(
    const std::filesystem::path& path,
    TextureMetadata& metadata) {

    std::ifstream input(
        path,
        std::ios::binary);

    unsigned char first = 0;
    unsigned char second = 0;

    input.read(
        reinterpret_cast<char*>(&first),
        1);

    input.read(
        reinterpret_cast<char*>(&second),
        1);

    if (!input ||
        first != 0xFF ||
        second != 0xD8) {
        return false;
    }

    while (input) {
        unsigned char prefix = 0;

        input.read(
            reinterpret_cast<char*>(&prefix),
            1);

        if (!input) break;
        if (prefix != 0xFF) continue;

        unsigned char marker = 0;

        do {
            input.read(
                reinterpret_cast<char*>(&marker),
                1);
        } while (
            input &&
            marker == 0xFF);

        if (!input) break;

        if (marker == 0xD9 ||
            marker == 0xDA) {
            break;
        }

        if (marker >= 0xD0 &&
            marker <= 0xD7) {
            continue;
        }

        unsigned char length_bytes[2]{};

        input.read(
            reinterpret_cast<char*>(
                length_bytes),
            2);

        if (!input) break;

        const std::uint16_t length =
            static_cast<std::uint16_t>(
                (static_cast<
                    std::uint16_t>(
                        length_bytes[0])
                    << 8u) |
                length_bytes[1]);

        if (length < 2) {
            return false;
        }

        if (is_jpeg_sof_marker(marker)) {
            unsigned char frame[5]{};

            input.read(
                reinterpret_cast<char*>(
                    frame),
                5);

            if (!input) return false;

            metadata.height =
                static_cast<
                    std::uint32_t>(
                        (frame[1] << 8u) |
                        frame[2]);

            metadata.width =
                static_cast<
                    std::uint32_t>(
                        (frame[3] << 8u) |
                        frame[4]);

            metadata.format = "jpeg";

            return
                metadata.width != 0 &&
                metadata.height != 0;
        }

        input.seekg(
            static_cast<std::streamoff>(
                length - 2),
            std::ios::cur);
    }

    return false;
}

TextureMetadata inspect_texture(
    const std::filesystem::path& path) {

    TextureMetadata metadata;

    const auto extension =
        lowercase(
            path.extension().string());

    bool parsed = false;

    if (extension == ".png") {
        parsed =
            read_png_metadata(
                path,
                metadata);
    } else if (
        extension == ".bmp") {
        parsed =
            read_bmp_metadata(
                path,
                metadata);
    } else if (
        extension == ".tga") {
        parsed =
            read_tga_metadata(
                path,
                metadata);
    } else if (
        extension == ".jpg" ||
        extension == ".jpeg") {
        parsed =
            read_jpeg_metadata(
                path,
                metadata);
    }

    if (!parsed) {
        metadata.format =
            extension.empty()
                ? "unknown"
                : extension.substr(1);
    }

    return metadata;
}

struct WavMetadata {
    bool parsed{false};
    std::uint16_t format_tag{0};
    std::uint16_t channels{0};
    std::uint32_t sample_rate{0};
    std::uint16_t bits_per_sample{0};
    std::uint64_t data_bytes{0};
};

WavMetadata inspect_wav(
    const std::filesystem::path& path) {

    WavMetadata metadata;

    std::ifstream input(
        path,
        std::ios::binary);

    unsigned char riff[12]{};

    if (!input.read(
            reinterpret_cast<char*>(riff),
            12)) {
        return metadata;
    }

    if (!(riff[0] == 'R' &&
          riff[1] == 'I' &&
          riff[2] == 'F' &&
          riff[3] == 'F' &&
          riff[8] == 'W' &&
          riff[9] == 'A' &&
          riff[10] == 'V' &&
          riff[11] == 'E')) {
        return metadata;
    }

    bool saw_fmt = false;
    bool saw_data = false;

    while (input) {
        unsigned char header[8]{};

        if (!input.read(
                reinterpret_cast<char*>(
                    header),
                8)) {
            break;
        }

        const std::uint32_t size =
            read_u32_le(
                header + 4);

        const std::string_view id{
            reinterpret_cast<
                const char*>(
                    header),
            4};

        if (id == "fmt ") {
            std::vector<
                unsigned char> buffer(
                    std::min<
                        std::uint32_t>(
                            size,
                            64u));

            if (!input.read(
                    reinterpret_cast<char*>(
                        buffer.data()),
                    static_cast<
                        std::streamsize>(
                            buffer.size()))) {
                break;
            }

            if (buffer.size() >= 16) {
                metadata.format_tag =
                    read_u16_le(
                        buffer.data());

                metadata.channels =
                    read_u16_le(
                        buffer.data() + 2);

                metadata.sample_rate =
                    read_u32_le(
                        buffer.data() + 4);

                metadata.bits_per_sample =
                    read_u16_le(
                        buffer.data() + 14);

                saw_fmt = true;
            }

            if (size >
                buffer.size()) {

                input.seekg(
                    static_cast<
                        std::streamoff>(
                            size -
                            buffer.size()),
                    std::ios::cur);
            }
        } else if (
            id == "data") {

            metadata.data_bytes = size;
            saw_data = true;

            input.seekg(
                static_cast<
                    std::streamoff>(size),
                std::ios::cur);
        } else {
            input.seekg(
                static_cast<
                    std::streamoff>(size),
                std::ios::cur);
        }

        if ((size & 1u) != 0u) {
            input.seekg(
                1,
                std::ios::cur);
        }

        if (saw_fmt &&
            saw_data) {
            break;
        }
    }

    metadata.parsed =
        saw_fmt && saw_data;

    return metadata;
}

} // namespace

ImportResult texture_source_importer(
    const ImportContext& context) {

    ImportResult result;
    std::filesystem::path source;

    if (!stage_source(
            context,
            result,
            source)) {
        return result;
    }

    const auto metadata =
        inspect_texture(
            context.asset->source_path);

    const auto descriptor =
        context.cache_directory /
        "texture.nasset";

    std::ostringstream text;

    text
        << "NENGINE_TEXTURE 1\n"
        << "FORMAT "
        << std::quoted(
            metadata.format)
        << "\n"
        << "WIDTH "
        << metadata.width
        << "\n"
        << "HEIGHT "
        << metadata.height
        << "\n"
        << "COLOR_SPACE "
        << std::quoted("sRGB")
        << "\n"
        << "SOURCE "
        << std::quoted(
            source.filename()
                .generic_string())
        << "\n"
        << "END_TEXTURE\n";

    if (!write_descriptor(
            descriptor,
            text.str(),
            result,
            "texture-descriptor")) {
        return result;
    }

    result.success = true;
    result.message =
        metadata.width != 0 &&
        metadata.height != 0
            ? "texture staged with metadata"
            : "texture staged; dimensions unavailable";

    return result;
}

ImportResult material_source_importer(
    const ImportContext& context) {

    ImportResult result;
    std::filesystem::path source;

    if (!stage_source(
            context,
            result,
            source)) {
        return result;
    }

    std::ifstream input(
        context.asset->source_path,
        std::ios::binary);

    if (!input) {
        result.message =
            "could not open material source";
        return result;
    }

    std::string token;
    std::uint32_t version = 0;

    if (!(input >> token >> version) ||
        token != "NENGINE_MATERIAL" ||
        version != 1u) {

        result.message =
            "invalid material header";
        return result;
    }

    std::string texture_guid;

    if (!(input >> token) ||
        token != "BASE_COLOR_TEXTURE" ||
        !(input >> std::quoted(
            texture_guid))) {

        result.message =
            "material BASE_COLOR_TEXTURE is missing";
        return result;
    }

    const auto dependency =
        AssetGuid::parse(
            texture_guid);

    if (!dependency ||
        !dependency->valid()) {

        result.message =
            "material BASE_COLOR_TEXTURE GUID is invalid";
        return result;
    }

    if (!(input >> token) ||
        token != "END_MATERIAL") {

        result.message =
            "material terminator is missing";
        return result;
    }

    result.dependencies.push_back(
        *dependency);

    result.success = true;
    result.message =
        "material staged with base-color texture dependency";

    return result;
}

ImportResult model_source_importer(
    const ImportContext& context) {

    ImportResult result;
    std::filesystem::path source;

    if (!stage_source(
            context,
            result,
            source)) {
        return result;
    }

    const auto format = lowercase(
        context.asset->source_path.extension().string());

    if (format == ".gltf") {
        std::vector<GltfSidecar> sidecars;
        std::string sidecar_error;

        if (!collect_gltf_sidecars(
                context.asset->source_path,
                sidecars,
                &sidecar_error)) {
            result.message = sidecar_error;
            return result;
        }

        for (const auto& sidecar : sidecars) {
            const auto staged =
                context.cache_directory /
                sidecar.relative_path;

            std::error_code ec;
            std::filesystem::create_directories(
                staged.parent_path(), ec);
            if (ec) {
                result.message =
                    "glTF sidecar directory creation failed: " +
                    ec.message();
                return result;
            }

            std::filesystem::copy_file(
                sidecar.source_path,
                staged,
                std::filesystem::copy_options::overwrite_existing,
                ec);
            if (ec) {
                result.message =
                    "glTF sidecar copy failed: " +
                    ec.message();
                return result;
            }
            result.artifacts.push_back({staged, "model-sidecar"});
        }
    }

    const auto descriptor =
        context.cache_directory /
        "model.nasset";

    std::ostringstream text;

    text
        << "NENGINE_MODEL 1\n"
        << "FORMAT "
        << std::quoted(
            lowercase(
                context.asset
                    ->source_path
                    .extension()
                    .string()))
        << "\n"
        << "SOURCE "
        << std::quoted(
            source.filename()
                .generic_string())
        << "\n"
        << "SOURCE_BYTES "
        << context.asset->file_size
        << "\n"
        << "END_MODEL\n";

    if (!write_descriptor(
            descriptor,
            text.str(),
            result,
            "model-descriptor")) {
        return result;
    }

    result.success = true;
    result.message =
        format == ".gltf"
            ? "glTF source and external sidecars staged"
            : "model source staged";

    return result;
}

ImportResult audio_source_importer(
    const ImportContext& context) {

    ImportResult result;
    std::filesystem::path source;

    if (!stage_source(
            context,
            result,
            source)) {
        return result;
    }

    WavMetadata wav;

    const auto extension =
        lowercase(
            context.asset
                ->source_path
                .extension()
                .string());

    if (extension == ".wav") {
        wav = inspect_wav(
            context.asset->source_path);
    }

    const auto descriptor =
        context.cache_directory /
        "audio.nasset";

    std::ostringstream text;

    text
        << "NENGINE_AUDIO 1\n"
        << "FORMAT "
        << std::quoted(
            extension.empty()
                ? "unknown"
                : extension.substr(1))
        << "\n"
        << "CHANNELS "
        << wav.channels
        << "\n"
        << "SAMPLE_RATE "
        << wav.sample_rate
        << "\n"
        << "BITS_PER_SAMPLE "
        << wav.bits_per_sample
        << "\n"
        << "DATA_BYTES "
        << wav.data_bytes
        << "\n"
        << "SOURCE "
        << std::quoted(
            source.filename()
                .generic_string())
        << "\n"
        << "END_AUDIO\n";

    if (!write_descriptor(
            descriptor,
            text.str(),
            result,
            "audio-descriptor")) {
        return result;
    }

    result.success = true;
    result.message =
        wav.parsed
            ? "audio staged with WAV metadata"
            : "audio source staged";

    return result;
}

ImportResult shader_source_importer(
    const ImportContext& context) {

    ImportResult result;

    if (!context.asset ||
        !context.importer) {

        result.message =
            "invalid import context";
        return result;
    }

    if ((context.asset->file_size %
         sizeof(std::uint32_t)) != 0 ||
        context.asset->file_size <
            5u * sizeof(std::uint32_t)) {

        result.message =
            "SPIR-V binary size is invalid";
        return result;
    }

    std::ifstream input(
        context.asset->source_path,
        std::ios::binary);

    std::uint32_t magic = 0;

    input.read(
        reinterpret_cast<char*>(&magic),
        sizeof(magic));

    if (!input ||
        magic != 0x07230203u) {

        result.message =
            "SPIR-V magic is invalid";
        return result;
    }

    std::filesystem::path source;

    if (!stage_source(
            context,
            result,
            source)) {
        return result;
    }

    std::string stage =
        "unknown";

    const auto stage_extension =
        lowercase(
            context.asset
                ->source_path
                .stem()
                .extension()
                .string());

    if (stage_extension == ".vert" ||
        stage_extension == ".vs") {
        stage = "vertex";
    } else if (
        stage_extension == ".frag" ||
        stage_extension == ".fs") {
        stage = "fragment";
    }

    const auto descriptor =
        context.cache_directory /
        "shader.nasset";

    std::ostringstream text;

    text
        << "NENGINE_SHADER 1\n"
        << "FORMAT "
        << std::quoted("spirv")
        << "\n"
        << "STAGE "
        << std::quoted(stage)
        << "\n"
        << "WORDS "
        << (context.asset->file_size /
            sizeof(std::uint32_t))
        << "\n"
        << "SOURCE "
        << std::quoted(
            source.filename()
                .generic_string())
        << "\n"
        << "END_SHADER\n";

    if (!write_descriptor(
            descriptor,
            text.str(),
            result,
            "shader-descriptor")) {
        return result;
    }

    result.success = true;
    result.message =
        stage == "unknown"
            ? "SPIR-V shader staged; stage hint unavailable"
            : "SPIR-V shader staged";

    return result;
}

} // namespace nengine::assets
