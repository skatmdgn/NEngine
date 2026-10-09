#include "nengine/input/input_state.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace nengine::input {

std::size_t InputState::index(
    Key key) noexcept {

    const auto value =
        static_cast<std::size_t>(
            key);

    return value < key_count
        ? value
        : 0u;
}

void InputState::begin_frame() noexcept {
    pressed_.fill(false);
    released_.fill(false);
    pointer_.delta_x = 0.0f;
    pointer_.delta_y = 0.0f;
    pointer_.wheel_y = 0.0f;
    ++frame_index_;
}

void InputState::set_key(
    Key key,
    bool next) noexcept {

    if (key == Key::Unknown ||
        key == Key::Count) {
        return;
    }

    const auto i =
        index(key);

    if (held_[i] == next) {
        return;
    }

    held_[i] = next;

    if (next) {
        pressed_[i] = true;
    } else {
        released_[i] = true;
    }
}

void InputState::release_all() noexcept {
    for (std::size_t i = 1u;
         i < key_count;
         ++i) {

        if (!held_[i]) {
            continue;
        }

        held_[i] = false;
        released_[i] = true;
    }
}

bool InputState::held(
    Key key) const noexcept {

    if (key == Key::Unknown ||
        key == Key::Count) {
        return false;
    }

    return held_[index(key)];
}

bool InputState::pressed(
    Key key) const noexcept {

    if (key == Key::Unknown ||
        key == Key::Count) {
        return false;
    }

    return pressed_[index(key)];
}

bool InputState::released(
    Key key) const noexcept {

    if (key == Key::Unknown ||
        key == Key::Count) {
        return false;
    }

    return released_[index(key)];
}

void InputState::set_pointer_position(
    float x,
    float y) noexcept {

    if (!std::isfinite(x) ||
        !std::isfinite(y)) {
        return;
    }

    if (pointer_initialized_) {
        pointer_.delta_x +=
            x - pointer_.x;
        pointer_.delta_y +=
            y - pointer_.y;
    } else {
        pointer_initialized_ = true;
    }

    pointer_.x = x;
    pointer_.y = y;
}

void InputState::add_wheel(
    float delta_y) noexcept {

    if (std::isfinite(delta_y)) {
        pointer_.wheel_y +=
            delta_y;
    }
}

bool ActionMap::bind(
    std::string action,
    Key key) {

    if (action.empty() ||
        key == Key::Unknown ||
        key == Key::Count) {
        return false;
    }

    auto& list =
        bindings_[std::move(action)];

    const auto exists =
        std::find_if(
            list.begin(),
            list.end(),
            [key](const auto& binding) {
                return binding.key == key;
            });

    if (exists != list.end()) {
        return true;
    }

    list.push_back({key});
    return true;
}

bool ActionMap::unbind(
    std::string_view action) {

    return bindings_.erase(
        std::string{action}) != 0u;
}

bool ActionMap::contains(
    std::string_view action) const {

    return bindings_.contains(
        std::string{action});
}

const std::vector<ActionBinding>*
ActionMap::bindings(
    std::string_view action) const noexcept {

    const auto found =
        bindings_.find(
            std::string{action});

    return found == bindings_.end()
        ? nullptr
        : &found->second;
}

bool ActionMap::held(
    std::string_view action,
    const InputState& input) const noexcept {

    const auto* list =
        bindings(action);

    if (!list) return false;

    return std::any_of(
        list->begin(),
        list->end(),
        [&](const auto& binding) {
            return input.held(binding.key);
        });
}

bool ActionMap::pressed(
    std::string_view action,
    const InputState& input) const noexcept {

    const auto* list =
        bindings(action);

    if (!list) return false;

    return std::any_of(
        list->begin(),
        list->end(),
        [&](const auto& binding) {
            return input.pressed(binding.key);
        });
}

bool ActionMap::released(
    std::string_view action,
    const InputState& input) const noexcept {

    const auto* list =
        bindings(action);

    if (!list) return false;

    return std::any_of(
        list->begin(),
        list->end(),
        [&](const auto& binding) {
            return input.released(binding.key);
        });
}

} // namespace nengine::input
