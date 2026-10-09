#pragma once

#include <filesystem>
#include <iosfwd>
#include <optional>
#include <string>

#include "nengine/assets/asset_guid.hpp"
#include "nengine/assets/import_pipeline.hpp"
#include "nengine/core/math.hpp"

namespace nengine::render {

enum class MaterialAlphaMode {
    Opaque,
    Mask,
    Blend
};

struct MaterialAssetData {
    assets::AssetGuid base_color_texture{};
    assets::AssetGuid normal_texture{};
    assets::AssetGuid metallic_roughness_texture{};
    assets::AssetGuid emissive_texture{};
    assets::AssetGuid occlusion_texture{};

    float metallic_factor{1.0f};
    float roughness_factor{1.0f};
    float normal_scale{1.0f};
    float occlusion_strength{1.0f};
    core::Vec3 emissive_factor{};
    MaterialAlphaMode alpha_mode{
        MaterialAlphaMode::Opaque};
    float alpha_cutoff{0.5f};
    bool double_sided{false};

    bool valid() const noexcept {
        return
            base_color_texture.valid() &&
            metallic_factor >= 0.0f &&
            metallic_factor <= 1.0f &&
            roughness_factor >= 0.0f &&
            roughness_factor <= 1.0f &&
            normal_scale >= 0.0f &&
            occlusion_strength >= 0.0f &&
            occlusion_strength <= 1.0f &&
            alpha_cutoff >= 0.0f &&
            alpha_cutoff <= 1.0f;
    }
};

struct ResolvedMaterialAsset {
    assets::AssetGuid guid{};
    MaterialAssetData material{};
    std::filesystem::path source_path{};
};

bool read_material_asset(
    std::istream& input,
    MaterialAssetData& material,
    std::string* error = nullptr);

std::optional<ResolvedMaterialAsset>
resolve_material_asset(
    assets::AssetGuid guid,
    const assets::CachedArtifactSet& artifacts,
    std::string* error = nullptr);

} // namespace nengine::render
