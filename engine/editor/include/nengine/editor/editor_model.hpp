#pragma once

#include "nengine/core/component_registry.hpp"
#include "nengine/core/component_serialization.hpp"
#include "nengine/core/world.hpp"
#include "nengine/editor/command.hpp"
#include "nengine/editor/console_model.hpp"
#include "nengine/editor/editor_layout.hpp"
#include "nengine/editor/play_session.hpp"
#include "nengine/editor/project_session.hpp"
#include "nengine/editor/property_access.hpp"
#include "nengine/editor/selection.hpp"

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
    void sanitize_selection() { selection_.sanitize(presentation_world()); }

private:
    core::World world_{};
    core::ComponentRegistry component_registry_{};
    core::ComponentSerializationRegistry component_serialization_{};
    Selection selection_{};
    CommandStack commands_{};
    PlaySession play_session_{};
    ConsoleModel console_{};
    EditorLayoutState layout_{};
    ProjectSession project_{};
    PropertyAccessRegistry property_access_{};
};

} // namespace nengine::editor
