#pragma once

#include "nengine/core/component_registry.hpp"
#include "nengine/core/world.hpp"
#include "nengine/editor/command.hpp"
#include "nengine/editor/console_model.hpp"
#include "nengine/editor/play_session.hpp"
#include "nengine/editor/project_session.hpp"
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

    ProjectSession& project() noexcept { return project_; }
    const ProjectSession& project() const noexcept { return project_; }

    bool can_edit() const noexcept { return !play_session_.is_playing(); }
    void sanitize_selection() { selection_.sanitize(presentation_world()); }

private:
    core::World world_{};
    core::ComponentRegistry component_registry_{};
    Selection selection_{};
    CommandStack commands_{};
    PlaySession play_session_{};
    ConsoleModel console_{};
    ProjectSession project_{};
};

} // namespace nengine::editor
