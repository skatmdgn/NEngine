#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include "nengine/assets/asset_database.hpp"
#include "nengine/audio/audio_clip.hpp"
#include "nengine/assets/import_pipeline.hpp"
#include "nengine/render/sprite_animation.hpp"
#include "nengine/render/animation_controller.hpp"
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
    std::optional<assets::CachedArtifactSet> cooked_artifacts(
        assets::AssetGuid guid) const;
    std::optional<render::AnimationController> load_controller(
        assets::AssetGuid guid, std::string* error = nullptr) const;
    std::optional<render::SpriteAnimationClip> load_sprite_clip(
        assets::AssetGuid guid, std::string* error = nullptr) const;
    std::optional<audio::AudioClipData> load_wav(
        assets::AssetGuid guid, std::string* error = nullptr) const;
private:
    std::filesystem::path root_{};
    assets::AssetDatabase database_{};
};
} // namespace nengine::player
