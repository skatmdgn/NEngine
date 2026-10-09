#include "nengine/render/model_importer.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "nengine/assets/builtin_processors.hpp"
#include "nengine/render/asset_resources.hpp"
#include "nengine/render/decoded_texture.hpp"
#include "nengine/render/gltf_mesh.hpp"
#include "nengine/render/obj_material.hpp"

namespace nengine::render {
namespace {

const assets::ImportArtifact*
find_role(
    const std::vector<assets::ImportArtifact>& artifacts,
    std::string_view role) {

    const auto it =
        std::find_if(
            artifacts.begin(),
            artifacts.end(),
            [&](const auto& artifact) {
                return artifact.role == role;
            });

    return it == artifacts.end()
        ? nullptr
        : &*it;
}

bool write_text(
    const std::filesystem::path& path,
    std::string_view text) {

    std::error_code error;
    std::filesystem::create_directories(
        path.parent_path(),
        error);

    if (error) {
        return false;
    }

    std::ofstream output(
        path,
        std::ios::binary |
            std::ios::trunc);

    if (!output) {
        return false;
    }

    output.write(
        text.data(),
        static_cast<std::streamsize>(
            text.size()));

    return output.good();
}

bool write_rgba8_tga(
    const std::filesystem::path& path,
    const DecodedTextureData& texture) {

    if (!texture.valid() ||
        texture.width >
            std::numeric_limits<
                std::uint16_t>::max() ||
        texture.height >
            std::numeric_limits<
                std::uint16_t>::max()) {
        return false;
    }

    std::error_code error;
    std::filesystem::create_directories(
        path.parent_path(),
        error);

    if (error) {
        return false;
    }

    std::ofstream output(
        path,
        std::ios::binary |
            std::ios::trunc);

    if (!output) {
        return false;
    }

    std::array<std::uint8_t, 18> header{};
    header[2] = 2u;
    header[12] =
        static_cast<std::uint8_t>(
            texture.width & 0xffu);
    header[13] =
        static_cast<std::uint8_t>(
            (texture.width >> 8u) & 0xffu);
    header[14] =
        static_cast<std::uint8_t>(
            texture.height & 0xffu);
    header[15] =
        static_cast<std::uint8_t>(
            (texture.height >> 8u) & 0xffu);
    header[16] = 32u;
    header[17] = 0x28u;

    output.write(
        reinterpret_cast<const char*>(
            header.data()),
        static_cast<std::streamsize>(
            header.size()));

    std::array<std::uint8_t, 4> bgra{};

    for (std::size_t pixel = 0u;
         pixel < texture.rgba8.size();
         pixel += 4u) {

        bgra[0] =
            texture.rgba8[pixel + 2u];
        bgra[1] =
            texture.rgba8[pixel + 1u];
        bgra[2] =
            texture.rgba8[pixel + 0u];
        bgra[3] =
            texture.rgba8[pixel + 3u];

        output.write(
            reinterpret_cast<const char*>(
                bgra.data()),
            static_cast<std::streamsize>(
                bgra.size()));
    }

    return output.good();
}

std::string lower_extension(
    const std::filesystem::path& path);

double srgb_to_linear(
    std::uint8_t value) noexcept {

    const double encoded =
        static_cast<double>(value) /
        255.0;

    return encoded <= 0.04045
        ? encoded / 12.92
        : std::pow(
            (encoded + 0.055) /
                1.055,
            2.4);
}

std::uint8_t linear_to_srgb(
    double value) noexcept {

    value = std::clamp(
        value,
        0.0,
        1.0);

    const double encoded =
        value <= 0.0031308
            ? value * 12.92
            : 1.055 *
                std::pow(
                    value,
                    1.0 / 2.4) -
                0.055;

    return static_cast<std::uint8_t>(
        std::lround(
            std::clamp(
                encoded,
                0.0,
                1.0) *
            255.0));
}

bool build_obj_base_color(
    const ObjCookedMaterialSource& material,
    assets::AssetGuid temporary_guid,
    DecodedTextureData& texture,
    std::string* error) {

    texture = {};

    if (material.diffuse_texture) {
        ResolvedTextureAsset source;
        source.guid = temporary_guid;
        source.source_path =
            *material.diffuse_texture;

        auto format =
            lower_extension(
                source.source_path);

        if (!format.empty() &&
            format.front() == '.') {
            format.erase(
                format.begin());
        }

        source.metadata.format =
            std::move(format);
        source.metadata.color_space =
            "sRGB";

        if (!decode_texture_rgba8(
                source,
                texture,
                error)) {
            return false;
        }

        for (std::size_t pixel = 0u;
             pixel < texture.rgba8.size();
             pixel += 4u) {

            for (std::size_t channel = 0u;
                 channel < 3u;
                 ++channel) {

                const auto linear =
                    srgb_to_linear(
                        texture.rgba8[
                            pixel +
                            channel]) *
                    static_cast<double>(
                        material.diffuse[
                            channel]);

                texture.rgba8[
                    pixel +
                    channel] =
                    linear_to_srgb(
                        linear);
            }

            const auto alpha =
                static_cast<double>(
                    texture.rgba8[
                        pixel + 3u]) /
                255.0 *
                static_cast<double>(
                    material.alpha);

            texture.rgba8[
                pixel + 3u] =
                static_cast<std::uint8_t>(
                    std::lround(
                        std::clamp(
                            alpha,
                            0.0,
                            1.0) *
                        255.0));
        }

        texture.color_space =
            DecodedTextureColorSpace::SRgb;
        return true;
    }

    texture.width = 1u;
    texture.height = 1u;
    texture.color_space =
        DecodedTextureColorSpace::SRgb;
    texture.rgba8 = {
        linear_to_srgb(
            material.diffuse[0]),
        linear_to_srgb(
            material.diffuse[1]),
        linear_to_srgb(
            material.diffuse[2]),
        static_cast<std::uint8_t>(
            std::lround(
                std::clamp(
                    static_cast<double>(
                        material.alpha),
                    0.0,
                    1.0) *
                255.0))
    };

    return true;
}

struct CookedMapping {
    std::uint32_t slot{0};
    assets::AssetGuid material{};
};

bool append_cooked_material(
    const assets::ImportContext& context,
    assets::ImportResult& result,
    std::uint32_t slot,
    std::string_view texture_namespace,
    std::string_view material_namespace,
    std::string_view display_name,
    const DecodedTextureData& base_color,
    std::vector<CookedMapping>& mappings) {

    const auto texture_guid =
        assets::derive_subasset_guid(
            context.asset->guid,
            texture_namespace,
            slot);

    const auto material_guid =
        assets::derive_subasset_guid(
            context.asset->guid,
            material_namespace,
            slot);

    const auto directory =
        context.cache_directory /
        "subassets" /
        ("material_" +
         std::to_string(slot));

    const auto texture_source =
        directory /
        "base_color.tga";

    const auto texture_descriptor =
        directory /
        "base_color.nasset";

    const auto material_source =
        directory /
        "material.nmat";

    if (!write_rgba8_tga(
            texture_source,
            base_color)) {
        return false;
    }

    std::ostringstream texture_text;
    texture_text
        << "NENGINE_TEXTURE 1\n"
        << "FORMAT "
        << std::quoted("tga")
        << "\n"
        << "WIDTH "
        << base_color.width
        << "\n"
        << "HEIGHT "
        << base_color.height
        << "\n"
        << "COLOR_SPACE "
        << std::quoted("sRGB")
        << "\n"
        << "SOURCE "
        << std::quoted(
            texture_source
                .filename()
                .generic_string())
        << "\n"
        << "END_TEXTURE\n";

    if (!write_text(
            texture_descriptor,
            texture_text.str())) {
        return false;
    }

    std::ostringstream material_text;
    material_text
        << "NENGINE_MATERIAL 1\n"
        << "BASE_COLOR_TEXTURE "
        << std::quoted(
            texture_guid.to_string())
        << "\n"
        << "END_MATERIAL\n";

    if (!write_text(
            material_source,
            material_text.str())) {
        return false;
    }

    assets::GeneratedSubasset
        texture_subasset;

    texture_subasset.guid =
        texture_guid;
    texture_subasset.importer_id =
        "NEngine.Texture";
    texture_subasset.name =
        std::string{display_name} +
        " Base Color";
    texture_subasset.artifacts = {
        {
            texture_source,
            "source"
        },
        {
            texture_descriptor,
            "texture-descriptor"
        }
    };

    assets::GeneratedSubasset
        material_subasset;

    material_subasset.guid =
        material_guid;
    material_subasset.importer_id =
        "NEngine.Material";
    material_subasset.name =
        std::string{display_name};
    material_subasset.artifacts = {
        {
            material_source,
            "source"
        }
    };

    result.subassets.push_back(
        std::move(
            texture_subasset));

    result.subassets.push_back(
        std::move(
            material_subasset));

    mappings.push_back({
        slot,
        material_guid
    });

    return true;
}


bool append_generated_texture(
    const assets::ImportContext& context,
    assets::ImportResult& result,
    std::uint32_t slot,
    std::string_view guid_namespace,
    std::string_view file_stem,
    std::string_view display_name,
    const DecodedTextureData& texture,
    assets::AssetGuid& texture_guid) {

    if (!texture.valid()) {
        return false;
    }

    texture_guid =
        assets::derive_subasset_guid(
            context.asset->guid,
            guid_namespace,
            slot);

    const auto directory =
        context.cache_directory /
        "subassets" /
        ("material_" +
         std::to_string(slot));

    const auto source =
        directory /
        (std::string{file_stem} +
         ".tga");

    const auto descriptor =
        directory /
        (std::string{file_stem} +
         ".nasset");

    if (!write_rgba8_tga(
            source,
            texture)) {
        return false;
    }

    std::ostringstream text;
    text
        << "NENGINE_TEXTURE 1\n"
        << "FORMAT "
        << std::quoted("tga")
        << "\n"
        << "WIDTH "
        << texture.width
        << "\n"
        << "HEIGHT "
        << texture.height
        << "\n"
        << "COLOR_SPACE "
        << std::quoted(
            texture.color_space ==
                    DecodedTextureColorSpace::SRgb
                ? "sRGB"
                : "Linear")
        << "\n"
        << "SOURCE "
        << std::quoted(
            source.filename()
                .generic_string())
        << "\n"
        << "END_TEXTURE\n";

    if (!write_text(
            descriptor,
            text.str())) {
        return false;
    }

    assets::GeneratedSubasset subasset;
    subasset.guid =
        texture_guid;
    subasset.importer_id =
        "NEngine.Texture";
    subasset.name =
        std::string{display_name};
    subasset.artifacts = {
        {
            source,
            "source"
        },
        {
            descriptor,
            "texture-descriptor"
        }
    };

    result.subassets.push_back(
        std::move(
            subasset));

    return true;
}

const char* alpha_mode_text(
    MaterialAlphaMode mode) noexcept {

    switch (mode) {
    case MaterialAlphaMode::Opaque:
        return "OPAQUE";
    case MaterialAlphaMode::Mask:
        return "MASK";
    case MaterialAlphaMode::Blend:
        return "BLEND";
    }

    return "OPAQUE";
}

bool append_cooked_gltf_pbr_material(
    const assets::ImportContext& context,
    assets::ImportResult& result,
    std::uint32_t slot,
    const GltfPbrMaterialCookData& pbr,
    std::vector<CookedMapping>& mappings) {

    assets::AssetGuid base_color_guid;

    if (!append_generated_texture(
            context,
            result,
            slot,
            "gltf-base-color",
            "base_color",
            "glTF Base Color " +
                std::to_string(slot),
            pbr.base_color,
            base_color_guid)) {

        return false;
    }

    assets::AssetGuid normal_guid;
    assets::AssetGuid metallic_roughness_guid;
    assets::AssetGuid emissive_guid;
    assets::AssetGuid occlusion_guid;

    if (pbr.normal &&
        !append_generated_texture(
            context,
            result,
            slot,
            "gltf-normal",
            "normal",
            "glTF Normal " +
                std::to_string(slot),
            *pbr.normal,
            normal_guid)) {

        return false;
    }

    if (pbr.metallic_roughness &&
        !append_generated_texture(
            context,
            result,
            slot,
            "gltf-metallic-roughness",
            "metallic_roughness",
            "glTF Metallic Roughness " +
                std::to_string(slot),
            *pbr.metallic_roughness,
            metallic_roughness_guid)) {

        return false;
    }

    if (pbr.emissive &&
        !append_generated_texture(
            context,
            result,
            slot,
            "gltf-emissive",
            "emissive",
            "glTF Emissive " +
                std::to_string(slot),
            *pbr.emissive,
            emissive_guid)) {

        return false;
    }

    if (pbr.occlusion &&
        !append_generated_texture(
            context,
            result,
            slot,
            "gltf-occlusion",
            "occlusion",
            "glTF Occlusion " +
                std::to_string(slot),
            *pbr.occlusion,
            occlusion_guid)) {

        return false;
    }

    const auto material_guid =
        assets::derive_subasset_guid(
            context.asset->guid,
            "gltf-material",
            slot);

    const auto directory =
        context.cache_directory /
        "subassets" /
        ("material_" +
         std::to_string(slot));

    const auto material_source =
        directory /
        "material.nmat";

    std::ostringstream text;

    text
        << "NENGINE_MATERIAL 2\n"
        << "BASE_COLOR_TEXTURE "
        << std::quoted(
            base_color_guid
                .to_string())
        << "\n";

    if (normal_guid.valid()) {
        text
            << "NORMAL_TEXTURE "
            << std::quoted(
                normal_guid.to_string())
            << "\n";
    }

    if (metallic_roughness_guid.valid()) {
        text
            << "METALLIC_ROUGHNESS_TEXTURE "
            << std::quoted(
                metallic_roughness_guid
                    .to_string())
            << "\n";
    }

    if (emissive_guid.valid()) {
        text
            << "EMISSIVE_TEXTURE "
            << std::quoted(
                emissive_guid
                    .to_string())
            << "\n";
    }

    if (occlusion_guid.valid()) {
        text
            << "OCCLUSION_TEXTURE "
            << std::quoted(
                occlusion_guid
                    .to_string())
            << "\n";
    }

    text
        << "METALLIC_FACTOR "
        << pbr.metallic_factor
        << "\n"
        << "ROUGHNESS_FACTOR "
        << pbr.roughness_factor
        << "\n"
        << "NORMAL_SCALE "
        << pbr.normal_scale
        << "\n"
        << "OCCLUSION_STRENGTH "
        << pbr.occlusion_strength
        << "\n"
        << "EMISSIVE_FACTOR "
        << pbr.emissive_factor.x
        << " "
        << pbr.emissive_factor.y
        << " "
        << pbr.emissive_factor.z
        << "\n"
        << "ALPHA_MODE "
        << std::quoted(
            alpha_mode_text(
                pbr.alpha_mode))
        << "\n"
        << "ALPHA_CUTOFF "
        << pbr.alpha_cutoff
        << "\n"
        << "DOUBLE_SIDED "
        << (pbr.double_sided
                ? 1
                : 0)
        << "\n"
        << "END_MATERIAL\n";

    if (!write_text(
            material_source,
            text.str())) {

        return false;
    }

    assets::GeneratedSubasset material_subasset;
    material_subasset.guid =
        material_guid;
    material_subasset.importer_id =
        "NEngine.Material";
    material_subasset.name =
        "glTF Material " +
        std::to_string(slot);
    material_subasset.artifacts = {
        {
            material_source,
            "source"
        }
    };

    result.subassets.push_back(
        std::move(
            material_subasset));

    mappings.push_back({
        slot,
        material_guid
    });

    return true;
}

std::string lower_extension(
    const std::filesystem::path& path) {

    auto extension =
        path.extension().string();

    std::transform(
        extension.begin(),
        extension.end(),
        extension.begin(),
        [](unsigned char ch) {
            return static_cast<char>(
                std::tolower(ch));
        });

    return extension;
}

} // namespace

assets::ImportResult model_asset_importer(
    const assets::ImportContext& context) {

    auto result =
        assets::model_source_importer(
            context);

    if (!result.success ||
        !context.asset ||
        !context.importer) {
        return result;
    }

    const auto format =
        lower_extension(
            context.asset->source_path);

    if (format != ".gltf" &&
        format != ".glb" &&
        format != ".obj") {
        return result;
    }

    const auto* staged_source =
        find_role(
            result.artifacts,
            "source");

    if (!staged_source) {
        result.success = false;
        result.message =
            "model import is missing staged source";
        return result;
    }

    ResolvedModelAsset model;
    model.guid =
        context.asset->guid;
    model.source_path =
        staged_source->path;
    model.metadata.format =
        format;

    std::vector<CookedMapping> mappings;

    if (format == ".obj") {
        std::vector<
            ObjCookedMaterialSource>
            obj_materials;

        std::string material_error;

        if (!load_obj_material_sources(
                model,
                obj_materials,
                &material_error)) {

            result.success = false;
            result.message =
                "OBJ material discovery failed: " +
                material_error;
            return result;
        }

        for (const auto& material :
             obj_materials) {

            DecodedTextureData
                base_color;

            const auto temporary_guid =
                assets::derive_subasset_guid(
                    context.asset->guid,
                    "obj-source-texture",
                    material.slot);

            std::string decode_error;

            if (!build_obj_base_color(
                    material,
                    temporary_guid,
                    base_color,
                    &decode_error)) {

                // Keep geometry importable if an MTL texture uses a format
                // outside the current decoded-texture subset.
                continue;
            }

            if (!append_cooked_material(
                    context,
                    result,
                    material.slot,
                    "obj-base-color",
                    "obj-material",
                    material.name,
                    base_color,
                    mappings)) {

                result.success = false;
                result.message =
                    "could not write cooked OBJ material subassets";
                return result;
            }
        }
    } else {
        std::vector<std::uint32_t>
            material_slots;

        std::string discovery_error;

        if (!discover_gltf_material_slots(
                model,
                material_slots,
                &discovery_error)) {

            result.success = false;
            result.message =
                "glTF material discovery failed: " +
                discovery_error;
            return result;
        }

        for (const auto slot :
             material_slots) {

            GltfPbrMaterialCookData
                pbr;

            std::string decode_error;

            if (!decode_gltf_pbr_material(
                    model,
                    slot,
                    pbr,
                    &decode_error)) {

                // Keep geometry importable when a material uses image
                // encodings/extensions outside the current cooking subset.
                continue;
            }

            if (!append_cooked_gltf_pbr_material(
                    context,
                    result,
                    slot,
                    pbr,
                    mappings)) {

                result.success = false;
                result.message =
                    "could not write cooked glTF Material v2 subassets";
                return result;
            }
        }
    }

    if (!mappings.empty()) {
        const auto mapping_path =
            context.cache_directory /
            "model-materials.nasset";

        std::ostringstream text;
        text
            << "NENGINE_MODEL_MATERIALS 1\n"
            << "MATERIALS "
            << mappings.size()
            << "\n";

        for (const auto& mapping :
             mappings) {

            text
                << "MATERIAL "
                << mapping.slot
                << " "
                << std::quoted(
                    mapping.material
                        .to_string())
                << "\n";
        }

        text
            << "END_MODEL_MATERIALS\n";

        if (!write_text(
                mapping_path,
                text.str())) {

            result.success = false;
            result.message =
                "could not write cooked model material map";
            return result;
        }

        result.artifacts.push_back({
            mapping_path,
            "model-material-map"
        });
    }

    result.message =
        "model staged; cooked " +
        std::to_string(
            mappings.size()) +
        (format == ".obj"
            ? " OBJ material(s) into generated subassets"
            : " glTF material(s) into generated subassets");

    return result;
}

bool read_cooked_model_material_map(
    const assets::CachedArtifactSet&
        model_artifacts,
    CookedModelMaterialMap& materials) {

    materials.clear();

    const auto* mapping =
        find_role(
            model_artifacts.artifacts,
            "model-material-map");

    if (!mapping) {
        return true;
    }

    std::ifstream input(
        mapping->path,
        std::ios::binary);

    if (!input) {
        return false;
    }

    std::string token;
    std::uint32_t version = 0u;

    if (!(input >> token >> version) ||
        token !=
            "NENGINE_MODEL_MATERIALS" ||
        version != 1u) {
        return false;
    }

    std::size_t count = 0u;

    if (!(input >> token >> count) ||
        token != "MATERIALS" ||
        count > 65536u) {
        return false;
    }

    materials.reserve(
        count);

    for (std::size_t i = 0u;
         i < count;
         ++i) {

        std::uint32_t slot = 0u;
        std::string guid_text;

        if (!(input >> token >> slot) ||
            token != "MATERIAL" ||
            !(input >>
                std::quoted(
                    guid_text))) {
            materials.clear();
            return false;
        }

        const auto guid =
            assets::AssetGuid::parse(
                guid_text);

        if (!guid ||
            !guid->valid() ||
            materials.contains(
                slot)) {

            materials.clear();
            return false;
        }

        materials.emplace(
            slot,
            *guid);
    }

    if (!(input >> token) ||
        token !=
            "END_MODEL_MATERIALS") {

        materials.clear();
        return false;
    }

    return true;
}

std::optional<assets::AssetGuid>
find_cooked_model_material(
    const assets::CachedArtifactSet&
        model_artifacts,
    std::uint32_t material_slot) {

    CookedModelMaterialMap
        materials;

    if (!read_cooked_model_material_map(
            model_artifacts,
            materials)) {
        return std::nullopt;
    }

    const auto it =
        materials.find(
            material_slot);

    return it ==
        materials.end()
        ? std::nullopt
        : std::optional<
            assets::AssetGuid>{
                it->second};
}

} // namespace nengine::render
