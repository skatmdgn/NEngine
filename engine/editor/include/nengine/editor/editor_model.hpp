#include <algorithm>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <utility>
#include <vector>
#include <cstdint>

#pragma once

#include "nengine/audio/audio_device.hpp"
#include "nengine/audio/clip_cache.hpp"
#include "nengine/audio/mix_snapshot.hpp"
#include "nengine/audio/playback.hpp"
#include "nengine/core/component_registry.hpp"
#include "nengine/core/component_serialization.hpp"
#include "nengine/core/world.hpp"
#include "nengine/editor/command.hpp"
#include "nengine/editor/component_factory.hpp"
#include "nengine/editor/console_model.hpp"
#include "nengine/editor/editor_layout.hpp"
#include "nengine/editor/play_session.hpp"
#include "nengine/editor/project_session.hpp"
#include "nengine/editor/property_access.hpp"
#include "nengine/editor/selection.hpp"
#include "nengine/input/input_state.hpp"
#include "nengine/physics/contact_events.hpp"
#include "nengine/physics/collision.hpp"
#include "nengine/render/sprite_animation.hpp"
#include "nengine/render/animation_controller_asset.hpp"
#include "nengine/ui/canvas_asset.hpp"
#include "nengine/ui/pointer_router.hpp"
#include "nengine/ui/keyboard_router.hpp"
#include "nengine/ui/rasterizer.hpp"
#include "nengine/ui/canvas_edit_history.hpp"
#include "nengine/scripting/managed_runtime.hpp"
#include "nengine/scripting/script_system.hpp"

namespace nengine::editor {

class EditorModel {
public:
    EditorModel();

    core::World& world() noexcept { return world_; }
    const core::World& world() const noexcept { return world_; }

    core::World& presentation_world() noexcept {
        if (auto* runtime = play_session_.runtime_world()) return *runtime;
        return world_;
    }

    const core::World& presentation_world() const noexcept {
        if (const auto* runtime = play_session_.runtime_world()) return *runtime;
        return world_;
    }

    core::ComponentRegistry& component_registry() noexcept {
        return component_registry_;
    }

    core::ComponentSerializationRegistry&
    component_serialization() noexcept {
        return component_serialization_;
    }

    const core::ComponentSerializationRegistry&
    component_serialization() const noexcept {
        return component_serialization_;
    }

    const core::ComponentRegistry& component_registry() const noexcept {
        return component_registry_;
    }

    Selection& selection() noexcept { return selection_; }
    const Selection& selection() const noexcept { return selection_; }

    CommandStack& commands() noexcept { return commands_; }
    const CommandStack& commands() const noexcept { return commands_; }

    ComponentFactoryRegistry&
    component_factories() noexcept {
        return component_factories_;
    }

    const ComponentFactoryRegistry&
    component_factories() const noexcept {
        return component_factories_;
    }

    PlaySession& play_session() noexcept { return play_session_; }
    const PlaySession& play_session() const noexcept { return play_session_; }

    ConsoleModel& console() noexcept { return console_; }
    const ConsoleModel& console() const noexcept { return console_; }

    EditorLayoutState& layout() noexcept { return layout_; }
    const EditorLayoutState& layout() const noexcept { return layout_; }

    ProjectSession& project() noexcept { return project_; }
    const ProjectSession& project() const noexcept { return project_; }

    PropertyAccessRegistry& property_access() noexcept {
        return property_access_;
    }

    const PropertyAccessRegistry& property_access() const noexcept {
        return property_access_;
    }

    bool can_edit() const noexcept { return !play_session_.is_playing(); }

    bool begin_play_mode();
    bool stop_play_mode();

    bool initialize_managed_runtime(
        const std::filesystem::path& hostfxr_path,
        const std::filesystem::path& runtime_config_path,
        const std::filesystem::path& assembly_path,
        std::string_view assembly_name,
        std::string* error = nullptr);

    bool reload_managed_runtime(
        const std::filesystem::path& assembly_path,
        std::string_view assembly_name,
        std::string* error = nullptr);

    void shutdown_managed_runtime() noexcept;

    bool managed_runtime_ready() const noexcept {
        return managed_runtime_.valid();
    }

    void set_input_state(
        const input::InputState& state) noexcept {
        input_state_ = state;
    }

    const input::InputState& input_state()
        const noexcept {
        return input_state_;
    }

    const audio::AudioMixSnapshot&
    audio_mix_snapshot() const noexcept {
        return audio_mix_snapshot_;
    }

    audio::AudioDeviceInfo
    audio_device_info() const {
        return audio_output_device_.info();
    }

    const audio::AudioClipCache&
    audio_clip_cache() const noexcept {
        return audio_clip_cache_;
    }

    void tick_runtime(
        double elapsed_seconds);

    // The optional Canvas asset is displayed in Play Mode's diagnostic
    // Scene View. The caller supplies Scene View pixel coordinates.
    // Authoring draft is separate from Play Mode's mutable Canvas.
    bool begin_ui_canvas_edit(assets::AssetGuid asset,
                              std::string* error = nullptr);
    bool has_ui_canvas_draft() const noexcept {
        return ui_edit_open_ && can_edit();
    }
    assets::AssetGuid ui_edit_asset() const noexcept { return ui_edit_asset_; }
    bool ui_edit_dirty() const { return ui_edit_open_ && ui_edit_history_.dirty(); }
    const std::vector<ui::Widget>& ui_edit_widgets() const noexcept {
        static const std::vector<ui::Widget> empty{};
        return has_ui_canvas_draft() ? ui_edit_history_.canvas().widgets()
                                     : empty;
    }
    ui::WidgetId selected_ui_widget() const noexcept {
        return ui_edit_selected_;
    }
    const ui::Widget* ui_edit_widget() const noexcept {
        if (!has_ui_canvas_draft()) return nullptr;
        return ui_edit_preview_
            ? ui_edit_preview_->find(ui_edit_selected_)
            : ui_edit_history_.canvas().find(ui_edit_selected_);
    }
    ui::UiLayout ui_edit_layout(float width, float height) const;
    ui::WidgetId select_ui_widget_at(float width, float height,
                                    core::Vec2 pointer);
    bool select_ui_widget(ui::WidgetId id) noexcept;
    bool toggle_ui_widget(ui::WidgetId id) noexcept;
    const std::vector<ui::WidgetId>& selected_ui_widgets() const noexcept {
        return ui_edit_selection_;
    }
    bool is_ui_widget_selected(ui::WidgetId id) const noexcept {
        return std::find(ui_edit_selection_.begin(), ui_edit_selection_.end(), id)
            != ui_edit_selection_.end();
    }
    bool ui_edit_set_name(std::string name);
    bool ui_edit_set_text(std::string text);
    bool ui_edit_set_visible(bool visible);
    bool ui_edit_set_interactable(bool interactable);
    bool ui_edit_set_anchors(ui::RectAnchors anchors);
    bool ui_edit_set_color(ui::UiColor color);
    bool ui_edit_apply_anchor_preset(
        ui::AnchorPreset preset, float viewport_width,
        float viewport_height);
    bool ui_edit_begin_drag();
    bool ui_edit_update_drag(core::Vec2 delta, bool resize);
    bool ui_edit_end_drag(bool commit);
    bool ui_edit_dragging() const noexcept { return ui_edit_drag_active_; }
    ui::WidgetId ui_edit_add_widget(ui::WidgetKind kind,
                                    ui::WidgetId parent = ui::invalid_widget);
    ui::WidgetId ui_edit_duplicate_selected();
    bool ui_edit_remove_selected();
    bool ui_edit_reparent_selected(ui::WidgetId parent);
    bool ui_edit_set_sibling_index(std::size_t index);
    bool save_ui_canvas_edit(std::string* error = nullptr);
    bool ui_edit_undo();
    bool ui_edit_redo();
    std::size_t ui_edit_undo_count() const noexcept {
        return ui_edit_history_.undo_count();
    }
    std::size_t ui_edit_redo_count() const noexcept {
        return ui_edit_history_.redo_count();
    }

    bool set_ui_canvas_asset(assets::AssetGuid asset);
    bool has_ui_canvas() const noexcept { return ui_canvas_.has_value(); }
    assets::AssetGuid ui_canvas_asset() const noexcept { return ui_canvas_asset_; }
    ui::UiLayout ui_layout(float width, float height) const;
    std::vector<ui::UiPointerEvent> route_ui_pointer(
        float width, float height, core::Vec2 pointer, bool pressed);
    void cancel_ui_pointer_capture() noexcept {
        ui_pointer_router_.cancel_capture();
    }
    std::vector<ui::UiPointerEvent> route_ui_keyboard(
        float width, float height,
        bool tab_held, bool enter_held, bool reverse_tab = false,
        bool space_held = false, bool escape_held = false);
    void reset_ui_keyboard() noexcept { ui_keyboard_router_.reset(); }
    ui::WidgetId focused_ui_widget() const noexcept {
        return ui_keyboard_router_.focused();
    }
    // Clicks remain observable by the next managed Update, then expire.
    bool ui_was_clicked(ui::WidgetId id) const noexcept {
        return ui_clicked_widgets_.contains(id);
    }
    const std::vector<ui::UiPointerEvent>& ui_events() const noexcept {
        return ui_events_;
    }


    // Set named controller parameters on a Play Mode SpriteAnimator.
    bool set_sprite_controller_bool(
        core::Entity entity, std::string parameter, bool value);

    const render::SpriteControllerRuntime& sprite_controllers()
        const noexcept { return sprite_controllers_; }

    void invalidate_runtime_asset_caches() noexcept {
        canvas_cache_.clear();
        ui_canvas_.reset();
        ui_canvas_reload_pending_ = ui_canvas_asset_.valid();
        ui_pointer_router_.reset();
        ui_keyboard_router_.reset();
        ui_clicked_widgets_.clear();
        sprite_animation_cache_.clear();
        sprite_controller_asset_cache_.clear();
        sprite_controllers_.clear();
        bound_sprite_controllers_.clear();
        audio_clip_cache_.clear();
    }

    void mark_scene_saved() noexcept {
        saved_scene_state_id_ =
            commands_.state_id();
    }

    bool scene_dirty() const noexcept {
        return commands_.state_id() !=
            saved_scene_state_id_;
    }

    void sanitize_selection() { selection_.sanitize(presentation_world()); }

private:
    core::World world_{};
    core::ComponentRegistry component_registry_{};
    core::ComponentSerializationRegistry component_serialization_{};
    Selection selection_{};
    CommandStack commands_{};
    ComponentFactoryRegistry component_factories_{};
    PlaySession play_session_{};
    ConsoleModel console_{};
    EditorLayoutState layout_{};
    ProjectSession project_{};
    PropertyAccessRegistry property_access_{};
    ui::CanvasAssetCache canvas_cache_{};
    ui::CanvasEditHistory ui_edit_history_{};
    assets::AssetGuid ui_edit_asset_{};
    bool ui_edit_open_{false};
    ui::WidgetId ui_edit_selected_{ui::invalid_widget};
    std::vector<ui::WidgetId> ui_edit_selection_{};
    bool ui_edit_drag_active_{false};
    ui::WidgetId ui_edit_drag_widget_{ui::invalid_widget};
    ui::RectAnchors ui_edit_drag_anchors_{};
    std::vector<std::pair<ui::WidgetId, ui::RectAnchors>>
        ui_edit_drag_targets_{};
    std::optional<ui::UiCanvas> ui_edit_preview_{};
    std::optional<ui::UiCanvas> ui_canvas_{};
    assets::AssetGuid ui_canvas_asset_{};
    bool ui_canvas_reload_pending_{false};
    ui::UiPointerRouter ui_pointer_router_{};
    ui::UiKeyboardRouter ui_keyboard_router_{};
    std::vector<ui::UiPointerEvent> ui_events_{};
    std::unordered_set<ui::WidgetId> ui_clicked_widgets_{};

    render::SpriteAnimationClipCache
        sprite_animation_cache_{};

    render::AnimationControllerAssetCache sprite_controller_asset_cache_{};
    render::SpriteControllerRuntime sprite_controllers_{};
    std::unordered_map<std::uint64_t, assets::AssetGuid>
        bound_sprite_controllers_{};
    void sync_sprite_controllers(core::World& runtime);
    void bind_managed_ui();


    audio::AudioClipCache
        audio_clip_cache_{};

    input::InputState input_state_{};

    scripting::ManagedRuntime
        managed_runtime_{};

    scripting::ManagedScriptSystem
        managed_script_system_{};

    physics::ContactTracker
        physics_contact_tracker_{};
    physics::ContactImpulseCache
        physics_impulse_cache_{};

    audio::AudioPlaybackSystem
        audio_playback_system_{};

    audio::AudioOutputDevice
        audio_output_device_{};

    audio::AudioMixSnapshot
        audio_mix_snapshot_{};

    std::uint64_t saved_scene_state_id_{0};
};

} // namespace nengine::editor
