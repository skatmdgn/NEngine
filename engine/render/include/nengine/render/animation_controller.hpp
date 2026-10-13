#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "nengine/assets/asset_guid.hpp"

namespace nengine::render {

// A small deterministic sprite animation state graph. Its transitions
// evaluate in declaration order, and at most one transition runs per tick.
struct AnimationState {
    std::string name{};
    assets::AssetGuid clip{};
    bool loop{true};
    float speed{1.0f};
};

struct AnimationTransition {
    std::string from{};
    std::string to{};
    std::string bool_parameter{};
    bool expected{true};
    float minimum_exit_seconds{0.0f};
};

struct AnimationController {
    std::string default_state{};
    std::vector<AnimationState> states{};
    std::vector<AnimationTransition> transitions{};

    const AnimationState* find(std::string_view name) const noexcept;
    bool validate(std::string* error = nullptr) const;
};

struct AnimationControllerPlayer {
    std::string state{};
    std::unordered_map<std::string, bool> bools{};

    void set_bool(std::string parameter, bool value);
    bool get_bool(std::string_view parameter) const;
    void reset() noexcept;

    // elapsed_seconds is the playback time of the current state.
    // Returns true when a new state is entered (including initialization).
    bool evaluate(const AnimationController& controller, float elapsed_seconds);
};

} // namespace nengine::render
