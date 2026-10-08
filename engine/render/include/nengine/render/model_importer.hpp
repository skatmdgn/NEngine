#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>

#include "nengine/assets/asset_guid.hpp"
#include "nengine/assets/import_pipeline.hpp"

namespace nengine::render {

// Render-aware model processor. It delegates ordinary source/sidecar staging
// to the Assets importer, then cooks supported glTF PBR base colors into
// stable generated Texture/Material subassets.
assets::ImportResult model_asset_importer(
    const assets::ImportContext& context);

using CookedModelMaterialMap =
    std::unordered_map<
        std::uint32_t,
        assets::AssetGuid>;

// Read the persistent model material table once. Callers that render every
// frame should cache the result against CachedArtifactSet::fingerprint.
bool read_cooked_model_material_map(
    const assets::CachedArtifactSet& model_artifacts,
    CookedModelMaterialMap& materials);

// Convenience one-shot lookup.
std::optional<assets::AssetGuid>
find_cooked_model_material(
    const assets::CachedArtifactSet& model_artifacts,
    std::uint32_t material_slot);

} // namespace nengine::render
