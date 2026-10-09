#include <iostream>

#include "nengine/input/input_state.hpp"

namespace {
int failures = 0;

void check(bool condition, const char* message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}
}

int main() {
    using namespace nengine::input;

    InputState input;

    input.begin_frame();
    input.set_key(Key::A, true);

    check(
        input.held(Key::A) &&
        input.pressed(Key::A) &&
        !input.released(Key::A),
        "press event exposes held and pressed state");

    input.begin_frame();

    check(
        input.held(Key::A) &&
        !input.pressed(Key::A) &&
        !input.released(Key::A),
        "held key persists while per-frame transitions clear");

    input.set_key(Key::A, false);

    check(
        !input.held(Key::A) &&
        input.released(Key::A),
        "release event clears held and exposes released state");

    input.begin_frame();
    input.set_key(Key::Space, true);
    input.set_key(Key::Space, false);

    check(
        !input.held(Key::Space) &&
        input.pressed(Key::Space) &&
        input.released(Key::Space),
        "press and release within one frame preserve both transitions");

    input.begin_frame();
    input.set_key(Key::LeftShift, true);
    input.set_key(Key::MouseLeft, true);
    input.release_all();

    check(
        !input.held(Key::LeftShift) &&
        !input.held(Key::MouseLeft) &&
        input.released(Key::LeftShift) &&
        input.released(Key::MouseLeft),
        "focus-loss release clears all held keyboard and mouse buttons");

    input.begin_frame();
    input.set_pointer_position(10.0f, 20.0f);
    input.set_pointer_position(13.5f, 18.0f);
    input.add_wheel(1.0f);
    input.add_wheel(-0.25f);

    check(
        input.pointer().x == 13.5f &&
        input.pointer().y == 18.0f &&
        input.pointer().delta_x == 3.5f &&
        input.pointer().delta_y == -2.0f &&
        input.pointer().wheel_y == 0.75f,
        "pointer position delta and wheel accumulate inside one frame");

    input.begin_frame();

    check(
        input.pointer().delta_x == 0.0f &&
        input.pointer().delta_y == 0.0f &&
        input.pointer().wheel_y == 0.0f,
        "pointer delta and wheel reset at frame boundary");

    ActionMap actions;

    check(
        actions.bind("Jump", Key::Space) &&
        actions.bind("Jump", Key::A) &&
        actions.bind("Fire", Key::MouseLeft) &&
        actions.contains("Jump"),
        "action map accepts multiple bindings per action");

    input.set_key(Key::A, true);

    check(
        actions.held("Jump", input) &&
        actions.pressed("Jump", input) &&
        !actions.released("Jump", input),
        "action state aggregates bound key transitions");

    input.set_key(Key::A, false);

    check(
        !actions.held("Jump", input) &&
        actions.pressed("Jump", input) &&
        actions.released("Jump", input),
        "action map preserves same-frame press/release transitions");

    check(
        actions.unbind("Jump") &&
        !actions.contains("Jump") &&
        !actions.held("Jump", input),
        "action bindings can be removed deterministically");

    if (failures != 0) {
        std::cerr << "NEngineInputTests: "
                  << failures
                  << " failure(s)\n";
        return 1;
    }

    std::cout << "NEngineInputTests: all checks passed\n";
    return 0;
}
