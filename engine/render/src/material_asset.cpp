#include "nengine/render/material_asset.hpp"

#include <fstream>
#include <iomanip>
#include <istream>
#include <string_view>
#include <utility>

namespace nengine::render {
namespace {

void set_error(
    std::string* error,
    std::string message) {

    if (error) {
        *error = std::move(message);
    }
}

const assets::ImportArtifact* find_role(
    const assets::CachedArtifactSet& artifacts,
    std::string_view role) {

    for (const auto& artifact :
         artifacts.artifacts) {

        if (artifact.role == role) {
            return &artifact;
        }
    }

    return nullptr;
}

} // namespace

bool read_material_asset(
    std::istream& input,
    MaterialAssetData& material,
    std::string* error) {

    MaterialAssetData parsed;
    std::string token;
    std::uint32_t version = 0u;

    if (!(input >> token >> version) ||
        token != "NENGINE_MATERIAL" ||
        (version != 1u &&
         version != 2u)) {

        set_error(
            error,
            "invalid material header");
        return false;
    }

    const auto parse_guid =
        [&](assets::AssetGuid& target,
            std::string_view field) {

            std::string guid_text;

            if (!(input >>
                    std::quoted(
                        guid_text))) {

                set_error(
                    error,
                    "material " +
                        std::string{field} +
                        " GUID is missing");
                return false;
            }

            const auto guid =
                assets::AssetGuid::parse(
                    guid_text);

            if (!guid ||
                !guid->valid()) {

                set_error(
                    error,
                    "material " +
                        std::string{field} +
                        " GUID is invalid");
                return false;
            }

            target = *guid;
            return true;
        };

    if (!(input >> token) ||
        token != "BASE_COLOR_TEXTURE" ||
        !parse_guid(
            parsed.base_color_texture,
            "BASE_COLOR_TEXTURE")) {

        if (error &&
            error->empty()) {
            *error =
                "material BASE_COLOR_TEXTURE is missing";
        }

        return false;
    }

    if (version == 1u) {
        if (!(input >> token) ||
            token != "END_MATERIAL") {

            set_error(
                error,
                "material terminator is missing");
            return false;
        }

        material = parsed;
        return true;
    }

    while (input >> token) {
        if (token == "END_MATERIAL") {
            if (!parsed.valid()) {
                set_error(
                    error,
                    "material numeric factors are outside valid range");
                return false;
            }

            material = parsed;
            return true;
        }

        if (token == "NORMAL_TEXTURE") {
            if (!parse_guid(
                    parsed.normal_texture,
                    token)) {
                return false;
            }
            continue;
        }

        if (token ==
            "METALLIC_ROUGHNESS_TEXTURE") {
            if (!parse_guid(
                    parsed.metallic_roughness_texture,
                    token)) {
                return false;
            }
            continue;
        }

        if (token == "EMISSIVE_TEXTURE") {
            if (!parse_guid(
                    parsed.emissive_texture,
                    token)) {
                return false;
            }
            continue;
        }

        if (token == "OCCLUSION_TEXTURE") {
            if (!parse_guid(
                    parsed.occlusion_texture,
                    token)) {
                return false;
            }
            continue;
        }

        if (token == "METALLIC_FACTOR") {
            if (!(input >>
                    parsed.metallic_factor)) {
                set_error(
                    error,
                    "material METALLIC_FACTOR is invalid");
                return false;
            }
            continue;
        }

        if (token == "ROUGHNESS_FACTOR") {
            if (!(input >>
                    parsed.roughness_factor)) {
                set_error(
                    error,
                    "material ROUGHNESS_FACTOR is invalid");
                return false;
            }
            continue;
        }

        if (token == "EMISSIVE_FACTOR") {
            if (!(input >>
                    parsed.emissive_factor.x >>
                    parsed.emissive_factor.y >>
                    parsed.emissive_factor.z)) {
                set_error(
                    error,
                    "material EMISSIVE_FACTOR is invalid");
                return false;
            }
            continue;
        }

        if (token == "ALPHA_MODE") {
            std::string mode;

            if (!(input >>
                    std::quoted(mode))) {
                set_error(
                    error,
                    "material ALPHA_MODE is invalid");
                return false;
            }

            if (mode == "OPAQUE") {
                parsed.alpha_mode =
                    MaterialAlphaMode::Opaque;
            } else if (mode == "MASK") {
                parsed.alpha_mode =
                    MaterialAlphaMode::Mask;
            } else if (mode == "BLEND") {
                parsed.alpha_mode =
                    MaterialAlphaMode::Blend;
            } else {
                set_error(
                    error,
                    "material ALPHA_MODE is unsupported");
                return false;
            }

            continue;
        }

        if (token == "ALPHA_CUTOFF") {
            if (!(input >>
                    parsed.alpha_cutoff)) {
                set_error(
                    error,
                    "material ALPHA_CUTOFF is invalid");
                return false;
            }
            continue;
        }

        if (token == "DOUBLE_SIDED") {
            std::uint32_t value = 0u;

            if (!(input >> value) ||
                value > 1u) {
                set_error(
                    error,
                    "material DOUBLE_SIDED must be 0 or 1");
                return false;
            }

            parsed.double_sided =
                value != 0u;
            continue;
        }

        set_error(
            error,
            "unknown material v2 field: " +
                token);
        return false;
    }

    set_error(
        error,
        "material terminator is missing");
    return false;
}

std::optional<ResolvedMaterialAsset>
resolve_material_asset(
    assets::AssetGuid guid,
    const assets::CachedArtifactSet& artifacts,
    std::string* error) {

    if (!guid.valid()) {
        set_error(
            error,
            "material AssetGuid is invalid");
        return std::nullopt;
    }

    const auto* source =
        find_role(
            artifacts,
            "source");

    if (!source) {
        set_error(
            error,
            "material cache is missing source artifact");
        return std::nullopt;
    }

    std::ifstream input(
        source->path,
        std::ios::binary);

    if (!input) {
        set_error(
            error,
            "could not open material source artifact");
        return std::nullopt;
    }

    ResolvedMaterialAsset resolved;
    resolved.guid = guid;
    resolved.source_path =
        source->path;

    if (!read_material_asset(
            input,
            resolved.material,
            error)) {
        return std::nullopt;
    }

    return resolved;
}

} // namespace nengine::render
