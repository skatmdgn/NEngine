#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

#include "nengine/core/scene.hpp"
#include "nengine/player/bundle.hpp"
#include "nengine/player/assets.hpp"
#include "nengine/player/ui_runtime.hpp"
#include "nengine/render/decoded_texture.hpp"
#include "nengine/audio/playback.hpp"
#include <unordered_map>
#include <unordered_set>
#include "nengine/physics/simulation.hpp"
#include "nengine/physics/contact_events.hpp"
#include "nengine/input/input_state.hpp"
#include "nengine/navigation/nav_mesh.hpp"
#include "nengine/scripting/managed_runtime.hpp"
#include "nengine/scripting/script_system.hpp"

namespace nengine::player {

struct PlayerFrameStats {
    std::size_t physics_steps{0};
    std::size_t integrated_3d{0};
    std::size_t integrated_2d{0};
    std::size_t physics_contacts{0};
    bool clamped_delta{false};
    audio::AudioPlaybackStats audio{};
    std::size_t sprite_sampled{0};
    std::size_t sprite_unresolved{0};
    std::size_t controller_transitions{0};
    std::size_t managed_fixed_updates{0};
    std::size_t managed_updates{0};
    std::size_t managed_errors{0};
    std::size_t contact_events{0};
};

// Isolated player World bootstrap. Never depends on the editor UI/model.
class PlayerRuntime {
public:
    // Call after load(); managed payloads are optional but require installed
    // hostfxr/.NET and the NEngine.API + Bridge assemblies.
    bool initialize_managed(std::string* error = nullptr);
    bool managed_ready() const noexcept { return managed_runtime_.valid(); }
    bool load(const std::filesystem::path& bundle_root,
              std::string* error = nullptr);
    void unload() noexcept;
    const core::World& world() const noexcept { return world_; }
    core::World& world() noexcept { return world_; }
    const BundleManifest& manifest() const noexcept { return manifest_; }
    bool ready() const noexcept { return ready_; }
    PlayerFrameStats advance(float delta_seconds);
    void set_paused(bool value) noexcept { paused_ = value; }
    input::InputState& input() noexcept { return input_; }
    const input::InputState& input() const noexcept { return input_; }
    void begin_input_frame() noexcept { input_.begin_frame(); }
    void update_ui(float width, float height, bool focused = true) {
        if (ready_) ui_.update(width, height, input_, focused);
    }
    input::ActionMap& actions() noexcept { return actions_; }
    const input::ActionMap& actions() const noexcept { return actions_; }
    bool paused() const noexcept { return paused_; }
    float fixed_delta_seconds() const noexcept { return 1.0f / 60.0f; }
    const PlayerAssets& assets() const noexcept { return assets_; }
    const navigation::NavMesh& navigation_mesh() const noexcept { return navigation_; }
    std::optional<navigation::NavPath> find_navigation_path(
        core::Vec3 from, core::Vec3 to,
        const navigation::NavQuery& query = {}) const {
        return navigation_.find_path(from, to, query);
    }
    PlayerUiRuntime& ui() noexcept { return ui_; }
    const PlayerUiRuntime& ui() const noexcept { return ui_; }
    std::size_t cached_ui_images() const noexcept { return ui_images_.size(); }
    const audio::AudioClipData* audio_clip(assets::AssetGuid guid) const noexcept;
    double elapsed_seconds() const noexcept { return elapsed_seconds_; }
    std::size_t entity_count() const noexcept { return world_.size(); }
    // Managed runtime is supported with bundled GameScripts.dll; this count
    // only describes ScriptBehaviour components awaiting an assembly.
    std::size_t pending_script_count() const noexcept;
private:
    // Resolve newly assigned C# clip GUIDs from the immutable export bundle
    // without repeatedly attempting to decode invalid/missing assets.
    void ensure_referenced_assets();
    const ui::UiRgbaImage* resolve_ui_image(assets::AssetGuid guid);
    std::unordered_set<assets::AssetGuid, assets::AssetGuidHash>
        missing_audio_clips_{};
    std::unordered_set<assets::AssetGuid, assets::AssetGuidHash>
        missing_sprite_clips_{};
    // Tracks which controller GUID is currently bound for each full Entity.
    std::unordered_map<std::uint64_t, assets::AssetGuid> controller_bindings_{};
    std::unordered_set<assets::AssetGuid, assets::AssetGuidHash>
        missing_controller_guids_{};

    core::World world_{};
    std::filesystem::path bundle_root_{};
    scripting::ManagedRuntime managed_runtime_{};
    scripting::ManagedScriptSystem script_system_{};
    input::InputState input_{};
    input::ActionMap actions_{};
    PlayerAssets assets_{};
    navigation::NavMesh navigation_{};
    PlayerUiRuntime ui_{};
    std::unordered_map<assets::AssetGuid, ui::UiRgbaImage,
        assets::AssetGuidHash> ui_images_{};
    std::unordered_set<assets::AssetGuid, assets::AssetGuidHash>
        missing_ui_images_{};
    BundleManifest manifest_{};
    bool ready_{false};
    bool paused_{false};
    double accumulator_{0.0};
    double elapsed_seconds_{0.0};
    physics::ContactImpulseCache physics_cache_{};
    physics::ContactTracker contact_tracker_{};
    audio::AudioPlaybackSystem audio_playback_{};
    render::SpriteControllerRuntime sprite_controllers_{};
    std::unordered_map<assets::AssetGuid, render::SpriteAnimationClip,
                       assets::AssetGuidHash> sprite_clips_{};
    std::unordered_map<assets::AssetGuid, audio::AudioClipData,
                       assets::AssetGuidHash> audio_clips_{};
};

} // namespace nengine::player
