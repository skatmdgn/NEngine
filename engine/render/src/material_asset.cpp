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
    std::uint32_t version = 0;

    if (!(input >> token >> version) ||
        token != "NENGINE_MATERIAL" ||
        version != 1u) {

        set_error(
            error,
            "invalid material header");
        return false;
    }

    std::string texture_guid;

    if (!(input >> token) ||
        token != "BASE_COLOR_TEXTURE" ||
        !(input >> std::quoted(
            texture_guid))) {

        set_error(
            error,
            "material BASE_COLOR_TEXTURE is missing");
        return false;
    }

    const auto parsed_guid =
        assets::AssetGuid::parse(
            texture_guid);

    if (!parsed_guid ||
        !parsed_guid->valid()) {

        set_error(
            error,
            "material BASE_COLOR_TEXTURE GUID is invalid");
        return false;
    }

    parsed.base_color_texture =
        *parsed_guid;

    if (!(input >> token) ||
        token != "END_MATERIAL") {

        set_error(
            error,
            "material terminator is missing");
        return false;
    }

    material =
        parsed;

    return true;
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
