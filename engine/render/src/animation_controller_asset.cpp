#include "nengine/render/animation_controller_asset.hpp"

#include <algorithm>
#include <fstream>
#include <unordered_set>
#include <utility>

#include "nengine/assets/builtin_processors.hpp"

namespace nengine::render {
namespace {
void set_error(std::string* error, std::string message) {
    if (error) *error = std::move(message);
}
} // namespace

std::optional<AnimationController> resolve_animation_controller(
    assets::AssetGuid guid, const assets::CachedArtifactSet& artifacts,
    std::string* error) {

    if (!guid.valid()) {
        set_error(error, "animation controller asset GUID is invalid");
        return std::nullopt;
    }

    const auto it = std::find_if(
        artifacts.artifacts.begin(), artifacts.artifacts.end(),
        [](const auto& artifact) { return artifact.role == "source"; });
    if (it == artifacts.artifacts.end()) {
        set_error(error, "animation controller cached source is missing");
        return std::nullopt;
    }

    std::ifstream input(it->path, std::ios::binary);
    if (!input) {
        set_error(error, "cannot open cached animation controller source");
        return std::nullopt;
    }

    AnimationController controller;
    if (!read_animation_controller(input, controller, error))
        return std::nullopt;
    return controller;
}

assets::ImportResult animation_controller_source_importer(
    const assets::ImportContext& context) {

    auto result = assets::copy_source_importer(context);
    if (!result.success) return result;

    assets::CachedArtifactSet stage;
    stage.artifacts = result.artifacts;
    const auto controller = resolve_animation_controller(
        context.asset ? context.asset->guid : assets::AssetGuid{},
        stage, &result.message);
    if (!controller) {
        result.success = false;
        return result;
    }

    std::unordered_set<assets::AssetGuid, assets::AssetGuidHash> used;
    for (const auto& state : controller->states) {
        if (used.insert(state.clip).second)
            result.dependencies.push_back(state.clip);
    }
    result.message = "animation controller staged with " +
        std::to_string(controller->states.size()) + " states";
    return result;
}

const AnimationController* AnimationControllerAssetCache::load(
    assets::AssetGuid guid, const assets::CachedArtifactSet& artifacts,
    std::string* error) {

    const auto it = entries_.find(guid);
    if (it != entries_.end() &&
        it->second.fingerprint == artifacts.fingerprint)
        return &it->second.controller;

    const auto controller = resolve_animation_controller(
        guid, artifacts, error);
    if (!controller) {
        entries_.erase(guid);
        return nullptr;
    }

    auto [next, ignored] = entries_.insert_or_assign(
        guid, Entry{artifacts.fingerprint, std::move(*controller)});
    (void)ignored;
    return &next->second.controller;
}

const AnimationController* AnimationControllerAssetCache::find(
    assets::AssetGuid guid) const noexcept {

    const auto it = entries_.find(guid);
    return it == entries_.end() ? nullptr : &it->second.controller;
}

void AnimationControllerAssetCache::clear() noexcept {
    entries_.clear();
}

} // namespace nengine::render
