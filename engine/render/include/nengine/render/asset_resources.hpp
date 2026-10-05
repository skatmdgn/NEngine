#pragma once

#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

#include "nengine/assets/asset_guid.hpp"
#include "nengine/assets/import_pipeline.hpp"

namespace nengine::render {

struct TextureAssetMetadata {
    std::string format{};
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::string color_space{};
    std::filesystem::path source_file{};
};

struct ModelAssetMetadata {
    std::string format{};
    std::uint64_t source_bytes{0};
    std::filesystem::path source_file{};
};

struct ResolvedTextureAsset {
    assets::AssetGuid guid{};
    TextureAssetMetadata metadata{};
    std::filesystem::path descriptor_path{};
    std::filesystem::path source_path{};
};

struct ResolvedModelAsset {
    assets::AssetGuid guid{};
    ModelAssetMetadata metadata{};
    std::filesystem::path descriptor_path{};
    std::filesystem::path source_path{};
};

struct ShaderAssetMetadata {
    std::string format{};
    std::string stage{};
    std::uint64_t words{0};
    std::filesystem::path source_file{};
};

struct ResolvedShaderAsset {
    assets::AssetGuid guid{};
    ShaderAssetMetadata metadata{};
    std::filesystem::path descriptor_path{};
    std::filesystem::path source_path{};
    std::vector<std::uint32_t> spirv{};
};

bool read_texture_asset_metadata(
    std::istream& input,
    TextureAssetMetadata& metadata,
    std::string* error = nullptr);

bool read_model_asset_metadata(
    std::istream& input,
    ModelAssetMetadata& metadata,
    std::string* error = nullptr);

bool read_shader_asset_metadata(
    std::istream& input,
    ShaderAssetMetadata& metadata,
    std::string* error = nullptr);

std::optional<ResolvedTextureAsset>
resolve_texture_asset(
    assets::AssetGuid guid,
    const assets::CachedArtifactSet& artifacts,
    std::string* error = nullptr);

std::optional<ResolvedModelAsset>
resolve_model_asset(
    assets::AssetGuid guid,
    const assets::CachedArtifactSet& artifacts,
    std::string* error = nullptr);

std::optional<ResolvedShaderAsset>
resolve_shader_asset(
    assets::AssetGuid guid,
    const assets::CachedArtifactSet& artifacts,
    std::string* error = nullptr);

} // namespace nengine::render
