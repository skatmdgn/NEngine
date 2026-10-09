#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace nengine::input {

enum class Key : std::uint16_t {
    Unknown = 0,
    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Digit0, Digit1, Digit2, Digit3, Digit4,
    Digit5, Digit6, Digit7, Digit8, Digit9,
    Space,
    Enter,
    Escape,
    Tab,
    Backspace,
    Up,
    Down,
    Left,
    Right,
    LeftShift,
    RightShift,
    LeftControl,
    RightControl,
    LeftAlt,
    RightAlt,
    MouseLeft,
    MouseRight,
    MouseMiddle,
    Count
};

struct PointerState {
    float x{0.0f};
    float y{0.0f};
    float delta_x{0.0f};
    float delta_y{0.0f};
    float wheel_y{0.0f};
};

class InputState {
public:
    void begin_frame() noexcept;

    void set_key(
        Key key,
        bool held) noexcept;

    void release_all() noexcept;

    bool held(
        Key key) const noexcept;

    bool pressed(
        Key key) const noexcept;

    bool released(
        Key key) const noexcept;

    void set_pointer_position(
        float x,
        float y) noexcept;

    void add_wheel(
        float delta_y) noexcept;

    const PointerState& pointer()
        const noexcept {
        return pointer_;
    }

    std::uint64_t frame_index()
        const noexcept {
        return frame_index_;
    }

private:
    static constexpr std::size_t key_count =
        static_cast<std::size_t>(
            Key::Count);

    static std::size_t index(
        Key key) noexcept;

    std::array<bool, key_count> held_{};
    std::array<bool, key_count> pressed_{};
    std::array<bool, key_count> released_{};
    PointerState pointer_{};
    bool pointer_initialized_{false};
    std::uint64_t frame_index_{0};
};

struct ActionBinding {
    Key key{Key::Unknown};
};

class ActionMap {
public:
    bool bind(
        std::string action,
        Key key);

    bool unbind(
        std::string_view action);

    bool contains(
        std::string_view action) const;

    bool held(
        std::string_view action,
        const InputState& input) const noexcept;

    bool pressed(
        std::string_view action,
        const InputState& input) const noexcept;

    bool released(
        std::string_view action,
        const InputState& input) const noexcept;

    const std::vector<ActionBinding>*
    bindings(
        std::string_view action) const noexcept;

private:
    std::unordered_map<
        std::string,
        std::vector<ActionBinding>>
        bindings_{};
};

} // namespace nengine::input
