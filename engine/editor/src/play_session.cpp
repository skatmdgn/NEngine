#include "nengine/editor/play_session.hpp"

namespace nengine::editor {

bool PlaySession::play(const core::World& editor_world) {
    if (state_ != PlayState::Editing) return false;
    runtime_world_.emplace(editor_world.clone());
    requested_steps_ = 0;
    state_ = PlayState::Playing;
    return true;
}

bool PlaySession::stop() {
    if (state_ == PlayState::Editing) return false;
    runtime_world_.reset();
    requested_steps_ = 0;
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

core::World* PlaySession::runtime_world() noexcept {
    return runtime_world_ ? &*runtime_world_ : nullptr;
}

const core::World* PlaySession::runtime_world() const noexcept {
    return runtime_world_ ? &*runtime_world_ : nullptr;
}

} // namespace nengine::editor
