#include "nengine/render/asset_resources.hpp"

#include <fstream>
#include <iomanip>
#include <istream>
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

bool read_texture_asset_metadata(
    std::istream& input,
    TextureAssetMetadata& metadata,
    std::string* error) {

    TextureAssetMetadata parsed;
    std::string token;
    std::uint32_t version = 0;

    if (!(input >> token >> version) ||
        token != "NENGINE_TEXTURE" ||
        version != 1) {

        set_error(
            error,
            "invalid texture descriptor header");
        return false;
    }

    if (!(input >> token) ||
        token != "FORMAT" ||
        !(input >> std::quoted(
            parsed.format))) {

        set_error(
            error,
            "missing texture format");
        return false;
    }

    if (!(input >> token) ||
        token != "WIDTH" ||
        !(input >> parsed.width)) {

        set_error(
            error,
            "missing texture width");
        return false;
    }

    if (!(input >> token) ||
        token != "HEIGHT" ||
        !(input >> parsed.height)) {

        set_error(
            error,
            "missing texture height");
        return false;
    }

    if (!(input >> token) ||
        token != "COLOR_SPACE" ||
        !(input >> std::quoted(
            parsed.color_space))) {

        set_error(
            error,
            "missing texture color space");
        return false;
    }

    std::string source;

    if (!(input >> token) ||
        token != "SOURCE" ||
        !(input >> std::quoted(source))) {

        set_error(
            error,
            "missing texture source artifact");
        return false;
    }

    if (!(input >> token) ||
        token != "END_TEXTURE") {

        set_error(
            error,
            "missing texture descriptor terminator");
        return false;
    }

    parsed.source_file =
        std::filesystem::path{
            std::move(source)};

    metadata = std::move(parsed);
    return true;
}

bool read_model_asset_metadata(
    std::istream& input,
    ModelAssetMetadata& metadata,
    std::string* error) {

    ModelAssetMetadata parsed;
    std::string token;
    std::uint32_t version = 0;

    if (!(input >> token >> version) ||
        token != "NENGINE_MODEL" ||
        version != 1) {

        set_error(
            error,
            "invalid model descriptor header");
        return false;
    }

    if (!(input >> token) ||
        token != "FORMAT" ||
        !(input >> std::quoted(
            parsed.format))) {

        set_error(
            error,
            "missing model format");
        return false;
    }

    std::string source;

    if (!(input >> token) ||
        token != "SOURCE" ||
        !(input >> std::quoted(source))) {

        set_error(
            error,
            "missing model source artifact");
        return false;
    }

    if (!(input >> token) ||
        token != "SOURCE_BYTES" ||
        !(input >> parsed.source_bytes)) {

        set_error(
            error,
            "missing model source byte count");
        return false;
    }

    if (!(input >> token) ||
        token != "END_MODEL") {

        set_error(
            error,
            "missing model descriptor terminator");
        return false;
    }

    parsed.source_file =
        std::filesystem::path{
            std::move(source)};

    metadata = std::move(parsed);
    return true;
}

std::optional<ResolvedTextureAsset>
resolve_texture_asset(
    assets::AssetGuid guid,
    const assets::CachedArtifactSet& artifacts,
    std::string* error) {

    if (!guid.valid()) {
        set_error(
            error,
            "texture AssetGuid is invalid");
        return std::nullopt;
    }

    const auto* descriptor =
        find_role(
            artifacts,
            "texture-descriptor");

    const auto* source =
        find_role(
            artifacts,
            "source");

    if (!descriptor || !source) {
        set_error(
            error,
            "texture cache is missing source or descriptor artifact");
        return std::nullopt;
    }

    std::ifstream input(
        descriptor->path,
        std::ios::binary);

    if (!input) {
        set_error(
            error,
            "could not open texture descriptor");
        return std::nullopt;
    }

    ResolvedTextureAsset result;
    result.guid = guid;
    result.descriptor_path =
        descriptor->path;
    result.source_path =
        source->path;

    if (!read_texture_asset_metadata(
            input,
            result.metadata,
            error)) {
        return std::nullopt;
    }

    if (result.metadata.width == 0 ||
        result.metadata.height == 0) {

        set_error(
            error,
            "texture dimensions are unavailable");
        return std::nullopt;
    }

    return result;
}

std::optional<ResolvedModelAsset>
resolve_model_asset(
    assets::AssetGuid guid,
    const assets::CachedArtifactSet& artifacts,
    std::string* error) {

    if (!guid.valid()) {
        set_error(
            error,
            "model AssetGuid is invalid");
        return std::nullopt;
    }

    const auto* descriptor =
        find_role(
            artifacts,
            "model-descriptor");

    const auto* source =
        find_role(
            artifacts,
            "source");

    if (!descriptor || !source) {
        set_error(
            error,
            "model cache is missing source or descriptor artifact");
        return std::nullopt;
    }

    std::ifstream input(
        descriptor->path,
        std::ios::binary);

    if (!input) {
        set_error(
            error,
            "could not open model descriptor");
        return std::nullopt;
    }

    ResolvedModelAsset result;
    result.guid = guid;
    result.descriptor_path =
        descriptor->path;
    result.source_path =
        source->path;

    if (!read_model_asset_metadata(
            input,
            result.metadata,
            error)) {
        return std::nullopt;
    }

    return result;
}

} // namespace nengine::render
