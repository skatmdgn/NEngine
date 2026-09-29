#pragma once

#include <cstdint>
#include <optional>

#include "nengine/core/world.hpp"

namespace nengine::editor {

enum class PlayState : std::uint8_t { Editing, Playing, Paused };

class PlaySession {
public:
    bool play(const core::World& editor_world);
    bool stop();
    bool pause();
    bool resume();
    bool step();

    PlayState state() const noexcept { return state_; }
    bool is_playing() const noexcept { return state_ != PlayState::Editing; }
    std::uint64_t requested_steps() const noexcept { return requested_steps_; }

    core::World* runtime_world() noexcept;
    const core::World* runtime_world() const noexcept;

private:
    PlayState state_{PlayState::Editing};
    std::optional<core::World> runtime_world_{};
    std::uint64_t requested_steps_{0};
};

} // namespace nengine::editor
