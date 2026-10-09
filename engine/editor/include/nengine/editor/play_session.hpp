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

    // Advance the runtime clock and return the number of fixed 60 Hz
    // simulation steps that should execute this host frame.
    std::uint32_t consume_simulation_steps(
        double elapsed_seconds,
        std::uint32_t max_steps = 8u) noexcept;

    double fixed_delta_seconds() const noexcept {
        return fixed_delta_seconds_;
    }

    PlayState state() const noexcept { return state_; }
    bool is_playing() const noexcept { return state_ != PlayState::Editing; }
    std::uint64_t requested_steps() const noexcept { return requested_steps_; }

    core::World* runtime_world() noexcept;
    const core::World* runtime_world() const noexcept;

private:
    PlayState state_{PlayState::Editing};
    std::optional<core::World> runtime_world_{};
    std::uint64_t requested_steps_{0};
    double accumulator_seconds_{0.0};
    double fixed_delta_seconds_{1.0 / 60.0};
};

} // namespace nengine::editor
