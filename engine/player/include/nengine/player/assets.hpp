#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include "nengine/assets/asset_database.hpp"
#include "nengine/audio/audio_clip.hpp"
#include "nengine/assets/import_pipeline.hpp"
#include "nengine/render/sprite_animation.hpp"
#include "nengine/render/animation_controller.hpp"
#include "nengine/navigation/nav_mesh.hpp"
namespace nengine::player {
// A read-only GUID catalog for exported Source Assets; unlike ProjectSession,
// this never writes missing .meta files or runs the editor import pipeline.
class PlayerAssets {
public:
    bool open(const std::filesystem::path& bundle_root,
              std::string* error = nullptr);
    void clear();
    std::optional<std::filesystem::path> source(
        assets::AssetGuid guid, std::string* error = nullptr) const;
    std::size_t size() const noexcept { return database_.size(); }
    std::size_t generated_subasset_count() const noexcept {
        return generated_subassets_.size();
    }
    std::optional<assets::CachedArtifactSet> cooked_artifacts(
        assets::AssetGuid guid) const;
    std::optional<render::AnimationController> load_controller(
        assets::AssetGuid guid, std::string* error = nullptr) const;
    std::optional<render::SpriteAnimationClip> load_sprite_clip(
        assets::AssetGuid guid, std::string* error = nullptr) const;
    bool load_navigation_default(navigation::NavMesh& output,
                                 std::string* error = nullptr) const;
    std::optional<audio::AudioClipData> load_wav(
        assets::AssetGuid guid, std::string* error = nullptr) const;
private:
    std::filesystem::path root_{};
    assets::AssetDatabase database_{};
    std::unordered_map<assets::AssetGuid, assets::CachedArtifactSet,
        assets::AssetGuidHash> generated_subassets_{};
};
} // namespace nengine::player
