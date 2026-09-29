#pragma once

#include "nengine/core/component_registry.hpp"
#include "nengine/core/world.hpp"
#include "nengine/editor/command.hpp"
#include "nengine/editor/play_session.hpp"
#include "nengine/editor/selection.hpp"

namespace nengine::editor {

class EditorModel {
public:
    EditorModel();

    core::World& world() noexcept { return world_; }
    const core::World& world() const noexcept { return world_; }

    core::ComponentRegistry& component_registry() noexcept { return component_registry_; }
    const core::ComponentRegistry& component_registry() const noexcept { return component_registry_; }

    Selection& selection() noexcept { return selection_; }
    const Selection& selection() const noexcept { return selection_; }

    CommandStack& commands() noexcept { return commands_; }
    const CommandStack& commands() const noexcept { return commands_; }

    PlaySession& play_session() noexcept { return play_session_; }
    const PlaySession& play_session() const noexcept { return play_session_; }

    bool can_edit() const noexcept { return !play_session_.is_playing(); }
    void sanitize_selection() { selection_.sanitize(world_); }

private:
    core::World world_{};
    core::ComponentRegistry component_registry_{};
    Selection selection_{};
    CommandStack commands_{};
    PlaySession play_session_{};
};

} // namespace nengine::editor
