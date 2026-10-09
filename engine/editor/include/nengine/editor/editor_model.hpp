#include <filesystem>
#include <string>
#include <string_view>

#pragma once

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
#include "nengine/render/sprite_animation.hpp"
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

    void tick_runtime(
        double elapsed_seconds);

    void invalidate_runtime_asset_caches() noexcept {
        sprite_animation_cache_.clear();
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
    render::SpriteAnimationClipCache
        sprite_animation_cache_{};

    scripting::ManagedRuntime
        managed_runtime_{};

    scripting::ManagedScriptSystem
        managed_script_system_{};

    std::uint64_t saved_scene_state_id_{0};
};

} // namespace nengine::editor
