#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

#include "nengine/core/scene.hpp"
#include "nengine/player/bundle.hpp"
#include "nengine/player/assets.hpp"
#include "nengine/audio/playback.hpp"
#include <unordered_map>
#include "nengine/physics/simulation.hpp"

namespace nengine::player {

struct PlayerFrameStats {
    std::size_t physics_steps{0};
    std::size_t integrated_3d{0};
    std::size_t integrated_2d{0};
    bool clamped_delta{false};
    audio::AudioPlaybackStats audio{};
};

// Isolated player World bootstrap. Never depends on the editor UI/model.
class PlayerRuntime {
public:
    bool load(const std::filesystem::path& bundle_root,
              std::string* error = nullptr);
    void unload() noexcept;
    const core::World& world() const noexcept { return world_; }
    core::World& world() noexcept { return world_; }
    const BundleManifest& manifest() const noexcept { return manifest_; }
    bool ready() const noexcept { return ready_; }
    PlayerFrameStats advance(float delta_seconds);
    float fixed_delta_seconds() const noexcept { return 1.0f / 60.0f; }
    const PlayerAssets& assets() const noexcept { return assets_; }
    const audio::AudioClipData* audio_clip(assets::AssetGuid guid) const noexcept;
    double elapsed_seconds() const noexcept { return elapsed_seconds_; }
    std::size_t entity_count() const noexcept { return world_.size(); }
    // Managed gameplay isn't yet attached in this standalone bootstrap.
    // Surface this explicitly rather than silently promising script execution.
    std::size_t pending_script_count() const noexcept;
private:
    core::World world_{};
    PlayerAssets assets_{};
    BundleManifest manifest_{};
    bool ready_{false};
    double accumulator_{0.0};
    double elapsed_seconds_{0.0};
    physics::ContactImpulseCache physics_cache_{};
    audio::AudioPlaybackSystem audio_playback_{};
    std::unordered_map<assets::AssetGuid, audio::AudioClipData,
                       assets::AssetGuidHash> audio_clips_{};
};

} // namespace nengine::player
