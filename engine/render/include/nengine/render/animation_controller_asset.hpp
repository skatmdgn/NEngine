#pragma once

#include <optional>
#include <string>
#include <unordered_map>

#include "nengine/assets/import_pipeline.hpp"
#include "nengine/render/animation_controller.hpp"

namespace nengine::render {

std::optional<AnimationController> resolve_animation_controller(
    assets::AssetGuid guid, const assets::CachedArtifactSet& artifacts,
    std::string* error = nullptr);

assets::ImportResult animation_controller_source_importer(
    const assets::ImportContext& context);

// Asset fingerprint invalidation is checked on every explicit load.
class AnimationControllerAssetCache {
public:
    const AnimationController* load(
        assets::AssetGuid guid, const assets::CachedArtifactSet& artifacts,
        std::string* error = nullptr);
    const AnimationController* find(assets::AssetGuid guid) const noexcept;
    void clear() noexcept;

private:
    struct Entry {
        std::string fingerprint{};
        AnimationController controller{};
    };
    std::unordered_map<assets::AssetGuid, Entry, assets::AssetGuidHash> entries_{};
};

} // namespace nengine::render
