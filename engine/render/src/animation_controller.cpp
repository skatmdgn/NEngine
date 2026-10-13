#include "nengine/render/animation_controller.hpp"
#include "nengine/render/components.hpp"

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

bool apply_sprite_controller(
    core::World& world, core::Entity entity,
    const AnimationController& definition,
    AnimationControllerPlayer& player) {

    if (!world.is_alive(entity) || !world.active(entity) ||
        !definition.validate())
        return false;

    auto* animator = world.get_component<SpriteAnimator>(
        entity, sprite_animator_type());
    if (!animator || !animator->enabled ||
        !world.get_component<SpriteRenderer>(entity, sprite_renderer_type()))
        return false;

    if (!player.evaluate(definition, animator->time_seconds))
        return false;

    const auto* state = definition.find(player.state);
    if (!state) return false;
    animator->clip = state->clip;
    animator->speed = state->speed;
    animator->loop = state->loop;
    animator->time_seconds = 0.0f;
    animator->playing = true;
    return true;
}

bool SpriteControllerRuntime::bind(
    core::Entity entity, AnimationController definition) {

    if (!entity.valid() || !definition.validate()) return false;
    bindings_.insert_or_assign(
        entity.value, Binding{std::move(definition), {}});
    return true;
}

bool SpriteControllerRuntime::set_bool(
    core::Entity entity, std::string parameter, bool value) {

    const auto it = bindings_.find(entity.value);
    if (it == bindings_.end() || parameter.empty()) return false;
    it->second.player.set_bool(std::move(parameter), value);
    return true;
}

bool SpriteControllerRuntime::unbind(core::Entity entity) noexcept {
    return bindings_.erase(entity.value) != 0u;
}

void SpriteControllerRuntime::clear() noexcept {
    bindings_.clear();
}

SpriteControllerUpdateStats SpriteControllerRuntime::evaluate(
    core::World& world) {

    SpriteControllerUpdateStats stats;
    for (auto it = bindings_.begin(); it != bindings_.end();) {
        const core::Entity entity{it->first};
        if (!world.is_alive(entity)) {
            it = bindings_.erase(it);
            ++stats.inactive_or_missing;
            continue;
        }
        auto* animator = world.get_component<SpriteAnimator>(
            entity, sprite_animator_type());
        if (!world.active(entity) || !animator ||
            !animator->enabled ||
            !world.get_component<SpriteRenderer>(
                entity, sprite_renderer_type())) {
            ++stats.inactive_or_missing;
            ++it;
            continue;
        }
        if (apply_sprite_controller(
                world, entity, it->second.controller, it->second.player))
            ++stats.transitions;
        ++it;
    }
    return stats;
}

} // namespace nengine::render
