#pragma once

#include <filesystem>
#include <iosfwd>
#include <optional>
#include <string>

#include "nengine/assets/asset_guid.hpp"
#include "nengine/assets/import_pipeline.hpp"

namespace nengine::render {

struct MaterialAssetData {
    assets::AssetGuid base_color_texture{};

    bool valid() const noexcept {
        return
            base_color_texture.valid();
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
