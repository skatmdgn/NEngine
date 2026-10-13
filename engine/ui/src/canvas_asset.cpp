#include "nengine/ui/canvas_asset.hpp"

#include <algorithm>
#include <fstream>
#include <utility>

#include "nengine/assets/builtin_processors.hpp"
#include "nengine/ui/canvas_serialization.hpp"

namespace nengine::ui {

std::optional<UiCanvas> resolve_canvas_asset(
    assets::AssetGuid guid, const assets::CachedArtifactSet& artifacts,
    std::string* error) {

    if (!guid.valid()) {
        if (error) *error = "invalid Canvas AssetGuid";
        return std::nullopt;
    }
    const auto it = std::find_if(
        artifacts.artifacts.begin(), artifacts.artifacts.end(),
        [](const auto& asset) { return asset.role == "source"; });
    if (it == artifacts.artifacts.end()) {
        if (error) *error = "Canvas cached source missing";
        return std::nullopt;
    }
    std::ifstream input(it->path, std::ios::binary);
    if (!input) {
        if (error) *error = "cannot open Canvas cached source";
        return std::nullopt;
    }
    UiCanvas canvas;
    if (!read_canvas(input, canvas, error))
        return std::nullopt;
    return canvas;
}

assets::ImportResult canvas_source_importer(
    const assets::ImportContext& context) {

    auto result = assets::copy_source_importer(context);
    if (!result.success) return result;

    assets::CachedArtifactSet staged;
    staged.artifacts = result.artifacts;
    const auto canvas = resolve_canvas_asset(
        context.asset ? context.asset->guid : assets::AssetGuid{},
        staged, &result.message);
    if (!canvas) {
        result.success = false;
        return result;
    }
    result.message = "Canvas staged with " +
        std::to_string(canvas->widgets().size()) + " widgets";
    return result;
}

const UiCanvas* CanvasAssetCache::load(
    assets::AssetGuid guid, const assets::CachedArtifactSet& artifacts,
    std::string* error) {

    const auto it = entries_.find(guid);
    if (it != entries_.end() &&
        it->second.fingerprint == artifacts.fingerprint)
        return &it->second.canvas;

    const auto canvas = resolve_canvas_asset(guid, artifacts, error);
    if (!canvas) {
        entries_.erase(guid);
        return nullptr;
    }
    auto [saved, ignored] = entries_.insert_or_assign(
        guid, Entry{artifacts.fingerprint, std::move(*canvas)});
    (void)ignored;
    return &saved->second.canvas;
}

const UiCanvas* CanvasAssetCache::find(assets::AssetGuid guid) const noexcept {
    const auto it = entries_.find(guid);
    return it == entries_.end() ? nullptr : &it->second.canvas;
}

} // namespace nengine::ui
