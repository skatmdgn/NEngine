#pragma once

#include <optional>
#include <string>
#include <unordered_map>

#include "nengine/assets/import_pipeline.hpp"
#include "nengine/ui/canvas.hpp"

namespace nengine::ui {

std::optional<UiCanvas> resolve_canvas_asset(
    assets::AssetGuid guid, const assets::CachedArtifactSet& artifacts,
    std::string* error = nullptr);

assets::ImportResult canvas_source_importer(
    const assets::ImportContext& context);

class CanvasAssetCache {
public:
    const UiCanvas* load(
        assets::AssetGuid guid, const assets::CachedArtifactSet& artifacts,
        std::string* error = nullptr);
    const UiCanvas* find(assets::AssetGuid guid) const noexcept;
    void clear() noexcept { entries_.clear(); }

private:
    struct Entry {
        std::string fingerprint{};
        UiCanvas canvas{};
    };
    std::unordered_map<assets::AssetGuid, Entry, assets::AssetGuidHash>
        entries_{};
};

} // namespace nengine::ui
