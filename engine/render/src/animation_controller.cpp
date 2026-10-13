#include "nengine/render/animation_controller.hpp"

#include <cmath>
#include <unordered_set>
#include <utility>

namespace nengine::render {

const AnimationState* AnimationController::find(
    std::string_view name) const noexcept {

    for (const auto& state : states)
        if (state.name == name) return &state;
    return nullptr;
}

bool AnimationController::validate(std::string* error) const {
    const auto fail = [&](const char* message) {
        if (error) *error = message;
        return false;
    };
    if (states.empty() || !find(default_state))
        return fail("animation controller default state is missing");

    std::unordered_set<std::string> names;
    for (const auto& state : states) {
        if (state.name.empty() || !names.insert(state.name).second ||
            !state.clip.valid() || !std::isfinite(state.speed) ||
            state.speed <= 0.0f)
            return fail("animation controller has an invalid/duplicate state");
    }
    for (const auto& edge : transitions) {
        if ((!edge.from.empty() && !find(edge.from)) ||
            !find(edge.to) || edge.bool_parameter.empty() ||
            !std::isfinite(edge.minimum_exit_seconds) ||
            edge.minimum_exit_seconds < 0.0f)
            return fail("animation controller transition is invalid");
    }
    if (error) error->clear();
    return true;
}

void AnimationControllerPlayer::set_bool(
    std::string parameter, bool value) {

    if (!parameter.empty()) bools[std::move(parameter)] = value;
}

bool AnimationControllerPlayer::get_bool(
    std::string_view parameter) const {

    const auto it = bools.find(std::string(parameter));
    return it != bools.end() && it->second;
}

void AnimationControllerPlayer::reset() noexcept {
    state.clear();
    bools.clear();
}

bool AnimationControllerPlayer::evaluate(
    const AnimationController& controller, float elapsed_seconds) {

    if (!std::isfinite(elapsed_seconds) || elapsed_seconds < 0.0f ||
        !controller.validate())
        return false;

    if (!controller.find(state)) {
        state = controller.default_state;
        return true;
    }

    for (const auto& edge : controller.transitions) {
        if ((!edge.from.empty() && edge.from != state) ||
            edge.to == state ||
            elapsed_seconds < edge.minimum_exit_seconds ||
            get_bool(edge.bool_parameter) != edge.expected)
            continue;

        state = edge.to;
        return true;
    }
    return false;
}

} // namespace nengine::render
