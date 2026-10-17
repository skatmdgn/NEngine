#include "nengine/input/input_state.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
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
        key == Key::Count ||
        action.size() > 64u ||
        !std::all_of(action.begin(), action.end(), [](unsigned char ch) {
            return (ch >= 'a' && ch <= 'z') ||
                   (ch >= 'A' && ch <= 'Z') ||
                   (ch >= '0' && ch <= '9') ||
                   ch == '_' || ch == '-' || ch == '.';
        })) {
        return false;
    }

    const auto existing_action = bindings_.find(action);
    if (existing_action == bindings_.end() && bindings_.size() >= 128u)
        return false;
    if (existing_action != bindings_.end() &&
        existing_action->second.size() >= 16u) {
        const auto exists = std::find_if(existing_action->second.begin(),
            existing_action->second.end(),
            [key](const auto& entry) { return entry.key == key; });
        return exists != existing_action->second.end();
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
    std::string_view action) const {

    const auto found =
        bindings_.find(
            std::string{action});

    return found == bindings_.end()
        ? nullptr
        : &found->second;
}

bool ActionMap::held(
    std::string_view action,
    const InputState& input) const {

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


namespace {

constexpr std::size_t max_action_map_bytes = 64u * 1024u;
constexpr std::size_t max_action_count = 128u;
constexpr std::size_t max_action_name_bytes = 64u;
constexpr std::size_t max_action_bindings = 16u;

// Explicit labels avoid persisting enum ordinals that may change as new
// input devices and keys are added in future engine versions.
constexpr std::array<std::string_view,
    static_cast<std::size_t>(Key::Count)> key_names{{
    "",
    "A",
    "B",
    "C",
    "D",
    "E",
    "F",
    "G",
    "H",
    "I",
    "J",
    "K",
    "L",
    "M",
    "N",
    "O",
    "P",
    "Q",
    "R",
    "S",
    "T",
    "U",
    "V",
    "W",
    "X",
    "Y",
    "Z",
    "Digit0",
    "Digit1",
    "Digit2",
    "Digit3",
    "Digit4",
    "Digit5",
    "Digit6",
    "Digit7",
    "Digit8",
    "Digit9",
    "Space",
    "Enter",
    "Escape",
    "Tab",
    "Backspace",
    "Up",
    "Down",
    "Left",
    "Right",
    "LeftShift",
    "RightShift",
    "LeftControl",
    "RightControl",
    "LeftAlt",
    "RightAlt",
    "MouseLeft",
    "MouseRight",
    "MouseMiddle"
}};

std::string_view key_name(Key key) noexcept {
    const auto index = static_cast<std::size_t>(key);
    return index < key_names.size() ? key_names[index] : std::string_view{};
}

Key parse_key(std::string_view name) noexcept {
    for (std::size_t index = 1u; index < key_names.size(); ++index)
        if (key_names[index] == name)
            return static_cast<Key>(index);
    return Key::Unknown;
}

bool valid_action_name(std::string_view name) noexcept {
    if (name.empty() || name.size() > max_action_name_bytes)
        return false;
    for (const unsigned char ch : name) {
        if ((ch >= 'A' && ch <= 'Z') ||
            (ch >= 'a' && ch <= 'z') ||
            (ch >= '0' && ch <= '9') ||
            ch == '_' || ch == '-' || ch == '.')
            continue;
        return false;
    }
    return true;
}

bool failure(std::string* error, const char* message) {
    if (error) *error = message;
    return false;
}

} // namespace

bool ActionMap::serialize(std::string& output, std::string* error) const {
    if (bindings_.size() > max_action_count)
        return failure(error, "action map has too many actions");
    std::vector<std::string_view> ordered;
    ordered.reserve(bindings_.size());
    for (const auto& [name, keys] : bindings_) {
        if (!valid_action_name(name) ||
            keys.empty() || keys.size() > max_action_bindings)
            return failure(error, "invalid action name or binding count");
        ordered.push_back(name);
    }
    std::sort(ordered.begin(), ordered.end());

    std::string encoded = "NENGINE_ACTION_MAP 1\n";
    for (const auto name : ordered) {
        encoded += "action ";
        encoded += name;
        for (const auto& binding : bindings_.at(std::string{name})) {
            const auto label = key_name(binding.key);
            if (label.empty())
                return failure(error, "unsupported action binding key");
            encoded += ' ';
            encoded += label;
        }
        encoded += '\n';
        if (encoded.size() > max_action_map_bytes)
            return failure(error, "action map exceeds size limit");
    }
    output = std::move(encoded);
    if (error) error->clear();
    return true;
}

bool ActionMap::deserialize(
    std::string_view source,
    ActionMap& output,
    std::string* error) {

    if (source.size() > max_action_map_bytes)
        return failure(error, "action map exceeds size limit");

    std::istringstream lines{std::string{source}};
    std::string line;
    if (!std::getline(lines, line))
        return failure(error, "missing action map header");
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line != "NENGINE_ACTION_MAP 1")
        return failure(error, "unsupported action map format or version");

    ActionMap parsed;
    std::size_t action_count = 0u;
    while (std::getline(lines, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream fields{line};
        std::string directive;
        std::string action;
        if (!(fields >> directive >> action) ||
            directive != "action" || !valid_action_name(action))
            return failure(error, "invalid action record");
        if (parsed.contains(action))
            return failure(error, "duplicate action name");
        if (++action_count > max_action_count)
            return failure(error, "too many action records");

        std::string token;
        std::size_t binding_count = 0u;
        while (fields >> token) {
            if (++binding_count > max_action_bindings)
                return failure(error, "too many action bindings");
            const auto key = parse_key(token);
            if (key == Key::Unknown)
                return failure(error, "unsupported action binding key");
            const auto* existing = parsed.bindings(action);
            if (existing && std::any_of(existing->begin(), existing->end(),
                [key](const ActionBinding& binding) {
                    return binding.key == key;
                }))
                return failure(error, "duplicate action binding");
            if (!parsed.bind(action, key))
                return failure(error, "failed to bind action");
        }
        if (binding_count == 0u)
            return failure(error, "action has no bindings");
    }
    output = std::move(parsed);
    if (error) error->clear();
    return true;
}

} // namespace nengine::input
