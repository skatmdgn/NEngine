#pragma once

#include <cstdint>
#include <optional>

#include "nengine/assets/asset_guid.hpp"
#include "nengine/assets/import_pipeline.hpp"

namespace nengine::render {

// Render-aware model processor. It delegates ordinary source/sidecar staging
// to the Assets importer, then cooks supported glTF PBR base colors into
// stable generated Texture/Material subassets.
assets::ImportResult model_asset_importer(
    const assets::ImportContext& context);

// Look up a cooked Material subasset GUID for one MeshSubmesh material slot.
// A missing entry is nonfatal and lets preview/runtime fall back to direct
// glTF decoding for unsupported material features.
std::optional<assets::AssetGuid>
find_cooked_model_material(
    const assets::CachedArtifactSet& model_artifacts,
    std::uint32_t material_slot);

} // namespace nengine::render
