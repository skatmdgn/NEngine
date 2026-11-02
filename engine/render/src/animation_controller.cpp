#include "nengine/render/animation_controller.hpp"
#include "nengine/render/components.hpp"
#include "nengine/assets/bounded_input.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <limits>
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
            edge.minimum_exit_seconds < 0.0f ||
            !std::isfinite(edge.blend_seconds) || edge.blend_seconds<0.f ||
            edge.blend_seconds>60.f)
            return fail("animation controller transition is invalid");
    }
    if (error) error->clear();
    return true;
}

bool write_animation_controller(
    std::ostream& output, const AnimationController& controller,
    std::string* error) {

    if (!controller.validate(error)) return false;
    std::ostringstream staged;
    staged << std::setprecision(std::numeric_limits<float>::max_digits10);
    const bool v2=std::any_of(controller.transitions.begin(),
        controller.transitions.end(),[](const auto& edge){return edge.blend_seconds>0.f;});
    staged << "NENGINE_ANIMATION_CONTROLLER " << (v2?2:1) << "\n"
           << "DEFAULT " << std::quoted(controller.default_state) << "\n"
           << "STATES " << controller.states.size() << "\n";
    for (const auto& state : controller.states) {
        staged << "STATE " << std::quoted(state.name) << ' '
               << std::quoted(state.clip.to_string()) << ' '
               << (state.loop ? 1 : 0) << ' ' << state.speed << "\n";
    }
    staged << "TRANSITIONS " << controller.transitions.size() << "\n";
    for (const auto& edge : controller.transitions) {
        staged << "TRANSITION " << std::quoted(edge.from) << ' '
               << std::quoted(edge.to) << ' '
               << std::quoted(edge.bool_parameter) << ' '
               << (edge.expected ? 1 : 0) << ' '
               << edge.minimum_exit_seconds;
        if(v2)staged << ' ' << edge.blend_seconds;
        staged << "\n";
    }
    staged << "END_ANIMATION_CONTROLLER\n";
    output << staged.str();
    if (!output.good()) {
        if (error) *error = "could not write animation controller";
        return false;
    }
    if (error) error->clear();
    return true;
}

bool read_animation_controller(
    std::istream& source, AnimationController& result,
    std::string* error) {

    const auto fail = [&](const char* message) {
        if (error) *error = message;
        return false;
    };
    std::string bytes;
    if (!assets::read_bounded_text(source, bytes, 4u * 1024u * 1024u))
        return fail("animation controller exceeds 4MiB or read failed");
    std::istringstream input{std::move(bytes)};
    std::string tag;
    unsigned version = 0;
    AnimationController candidate;
    if (!(input >> tag >> version) ||
        tag != "NENGINE_ANIMATION_CONTROLLER" || (version != 1u && version != 2u) ||
        !(input >> tag) || tag != "DEFAULT" ||
        !(input >> std::quoted(candidate.default_state)))
        return fail("invalid animation controller header/default");

    std::size_t state_count = 0;
    if (!(input >> tag >> state_count) || tag != "STATES" ||
        state_count == 0u || state_count > 4096u)
        return fail("invalid animation controller state count");

    for (std::size_t i = 0; i < state_count; ++i) {
        AnimationState state;
        std::string guid_text;
        int loop = -1;
        if (!(input >> tag) || tag != "STATE" ||
            !(input >> std::quoted(state.name) >>
                    std::quoted(guid_text) >> loop >> state.speed) ||
            (loop != 0 && loop != 1))
            return fail("malformed animation controller state");
        const auto guid = assets::AssetGuid::parse(guid_text);
        if (!guid || !guid->valid())
            return fail("invalid animation controller clip GUID");
        state.clip = *guid;
        state.loop = loop == 1;
        candidate.states.push_back(std::move(state));
    }

    std::size_t edge_count = 0;
    if (!(input >> tag >> edge_count) || tag != "TRANSITIONS" ||
        edge_count > 16384u)
        return fail("invalid animation controller transition count");

    for (std::size_t i = 0; i < edge_count; ++i) {
        AnimationTransition edge;
        int expected = -1;
        if (!(input >> tag) || tag != "TRANSITION" ||
            !(input >> std::quoted(edge.from) >> std::quoted(edge.to) >>
                    std::quoted(edge.bool_parameter) >> expected >>
                    edge.minimum_exit_seconds) ||
            (expected != 0 && expected != 1))
            return fail("malformed animation controller transition");
        if(version==2u && !(input >> edge.blend_seconds))
            return fail("malformed v2 animation transition blend seconds");
        edge.expected = expected == 1;
        candidate.transitions.push_back(std::move(edge));
    }

    if (!(input >> tag) || tag != "END_ANIMATION_CONTROLLER")
        return fail("animation controller terminator missing");
    if (input >> tag)
        return fail("trailing animation controller data");
    if (!candidate.validate(error)) return false;
    result = std::move(candidate);
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

float AnimationControllerPlayer::blend_weight() const noexcept {
    return blend_duration_seconds>0.f?
        std::clamp(blend_elapsed_seconds/blend_duration_seconds,0.f,1.f):1.f;
}

void AnimationControllerPlayer::reset() noexcept {
    state.clear();
    previous_state.clear();
    blend_elapsed_seconds=0.f;
    blend_duration_seconds=0.f;
    last_elapsed_seconds=0.f;
    bools.clear();
}

bool AnimationControllerPlayer::evaluate(
    const AnimationController& controller, float elapsed_seconds) {

    if (!std::isfinite(elapsed_seconds) || elapsed_seconds < 0.0f ||
        !controller.validate())
        return false;

    if (!controller.find(state)) {
        state = controller.default_state;
        previous_state.clear();
        blend_elapsed_seconds=0.f;
        blend_duration_seconds=0.f;
        last_elapsed_seconds=0.f;
        return true;
    }
    const float delta=std::max(0.f,elapsed_seconds-last_elapsed_seconds);
    if(blend_duration_seconds>0.f)
        blend_elapsed_seconds=std::min(blend_duration_seconds,
            blend_elapsed_seconds+delta);
    last_elapsed_seconds=elapsed_seconds;
    for (const auto& edge : controller.transitions) {
        if ((!edge.from.empty() && edge.from != state) ||
            edge.to == state ||
            elapsed_seconds < edge.minimum_exit_seconds ||
            get_bool(edge.bool_parameter) != edge.expected)
            continue;

        previous_state = state;
        state = edge.to;
        blend_elapsed_seconds=0.f;
        blend_duration_seconds=edge.blend_seconds;
        last_elapsed_seconds=0.f;
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

    const bool entered=player.evaluate(definition,animator->time_seconds);
    const auto* state=definition.find(player.state);
    if(!state)return false;
    const auto* previous=definition.find(player.previous_state);
    const float weight=player.blend_weight();
    const float previous_speed=previous?previous->speed:state->speed;
    animator->speed=previous_speed*(1.f-weight)+state->speed*weight;
    if(!entered)return false;
    animator->clip=state->clip;
    animator->loop=state->loop;
    animator->time_seconds=0.f;
    animator->playing=true;
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
