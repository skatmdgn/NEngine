#include "nengine/editor/play_session.hpp"

#include <algorithm>
#include <cmath>

namespace nengine::editor {

bool PlaySession::play(const core::World& editor_world) {
    if (state_ != PlayState::Editing) return false;
    runtime_world_.emplace(editor_world.clone());
    requested_steps_ = 0;
    accumulator_seconds_ = 0.0;
    state_ = PlayState::Playing;
    return true;
}

bool PlaySession::stop() {
    if (state_ == PlayState::Editing) return false;
    runtime_world_.reset();
    requested_steps_ = 0;
    accumulator_seconds_ = 0.0;
    state_ = PlayState::Editing;
    return true;
}

bool PlaySession::pause() {
    if (state_ != PlayState::Playing) return false;
    state_ = PlayState::Paused;
    return true;
}

bool PlaySession::resume() {
    if (state_ != PlayState::Paused) return false;
    state_ = PlayState::Playing;
    return true;
}

bool PlaySession::step() {
    if (state_ != PlayState::Paused || !runtime_world_) return false;
    ++requested_steps_;
    return true;
}

std::uint32_t PlaySession::consume_simulation_steps(
    double elapsed_seconds,
    std::uint32_t max_steps) noexcept {

    if (state_ == PlayState::Editing ||
        !runtime_world_ ||
        max_steps == 0u) {
        return 0u;
    }

    if (state_ == PlayState::Paused) {
        const auto steps =
            static_cast<std::uint32_t>(
                std::min<std::uint64_t>(
                    requested_steps_,
                    max_steps));

        requested_steps_ -=
            steps;

        return steps;
    }

    if (!std::isfinite(
            elapsed_seconds) ||
        elapsed_seconds <= 0.0) {
        return 0u;
    }

    // Avoid a debugger break/window stall causing an unbounded catch-up
    // spiral. At most max_steps are retained for one host frame.
    const auto clamped =
        std::min(
            elapsed_seconds,
            0.25);

    accumulator_seconds_ +=
        clamped;

    const auto max_accumulator =
        fixed_delta_seconds_ *
        static_cast<double>(
            max_steps);

    accumulator_seconds_ =
        std::min(
            accumulator_seconds_,
            max_accumulator);

    const auto available =
        static_cast<std::uint32_t>(
            accumulator_seconds_ /
            fixed_delta_seconds_);

    const auto steps =
        std::min(
            available,
            max_steps);

    accumulator_seconds_ -=
        static_cast<double>(
            steps) *
        fixed_delta_seconds_;

    return steps;
}

core::World* PlaySession::runtime_world() noexcept {
    return runtime_world_ ? &*runtime_world_ : nullptr;
}

const core::World* PlaySession::runtime_world() const noexcept {
    return runtime_world_ ? &*runtime_world_ : nullptr;
}

} // namespace nengine::editor
