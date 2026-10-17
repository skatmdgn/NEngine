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


    // Action bindings are project data: stable text, deterministic ordering,
    // transactional reads and fixed bounds prevent corrupted asset imports.
    ActionMap removable;
    check(removable.bind("Jump", Key::Space) &&
          removable.bind("Jump", Key::Enter) &&
          removable.unbind_key("Jump", Key::Space) &&
          removable.contains("Jump") &&
          !removable.unbind_key("Jump", Key::A) &&
          removable.unbind_key("Jump", Key::Enter) &&
          !removable.contains("Jump"), "remove one bound key and prune empty actions");

    ActionMap persistent;
    check(
        persistent.bind("Jump", Key::Space) &&
        persistent.bind("Jump", Key::A) &&
        persistent.bind("Fire", Key::MouseLeft),
        "versioned action map fixture binds supported keys");
    std::string saved;
    std::string persistence_error;
    check(
        persistent.serialize(saved, &persistence_error) &&
        saved == "NENGINE_ACTION_MAP 1\naction Fire MouseLeft\naction Jump Space A\n" &&
        persistence_error.empty(),
        "serialization is deterministic and uses stable human-readable key names");
    ActionMap restored;
    check(
        ActionMap::deserialize(saved, restored, &persistence_error) &&
        persistence_error.empty() &&
        restored.contains("Fire") &&
        restored.bindings("Jump") &&
        restored.bindings("Jump")->size() == 2u,
        "versioned action bindings round-trip without changing key order");

    input.begin_frame();
    input.set_key(Key::Space, true);
    check(
        restored.pressed("Jump", input) &&
        restored.held("Jump", input),
        "restored bindings drive input action transitions");

    ActionMap windows_lines;
    check(
        ActionMap::deserialize(
            "NENGINE_ACTION_MAP 1\r\naction Jump Space A\r\n",
            windows_lines, &persistence_error) &&
        windows_lines.contains("Jump"),
        "action map imports CRLF project files");

    const auto reject = [&](std::string_view invalid, const char* message) {
        ActionMap destination;
        destination.bind("Existing", Key::Enter);
        persistence_error.clear();
        check(
            !ActionMap::deserialize(invalid, destination, &persistence_error) &&
            !persistence_error.empty() &&
            destination.contains("Existing") &&
            !destination.contains("Jump") &&
            destination.held("Existing", input) == input.held(Key::Enter),
            message);
    };
    reject("", "empty action map is rejected transactionally");
    reject("NENGINE_ACTION_MAP 2\n", "future action map version is rejected");
    reject("NENGINE_ACTION_MAP 1\naction Jump\n",
        "action record without key is rejected");
    reject("NENGINE_ACTION_MAP 1\naction Jump Bogus\n",
        "unknown key token is rejected");
    reject("NENGINE_ACTION_MAP 1\naction Jump Space Space\n",
        "duplicate key binding is rejected");
    reject("NENGINE_ACTION_MAP 1\naction Jump Space\naction Jump A\n",
        "duplicate action name is rejected");
    reject("NENGINE_ACTION_MAP 1\naction Bad/Name Space\n",
        "unsafe action identifiers are rejected");
    reject("NENGINE_ACTION_MAP 1\naction Jump Space extra_data\n",
        "unexpected trailing tokens are rejected");
    reject("NENGINE_ACTION_MAP 1\n\naction Jump Space\n",
        "empty intermediate record is rejected");
    reject(std::string(65537u, 'x'),
        "oversized action map is rejected before parsing");
    std::string too_many_keys = "NENGINE_ACTION_MAP 1\naction Jump";
    for (char key = 'A'; key <= 'Q'; ++key) {
        too_many_keys += ' ';
        too_many_keys += key;
    }
    too_many_keys += '\n';
    reject(too_many_keys, "more than sixteen keys per action is rejected");
    std::string too_many_actions = "NENGINE_ACTION_MAP 1\n";
    for (int i = 0; i < 129; ++i)
        too_many_actions += "action A" + std::to_string(i) + " Space\n";
    reject(too_many_actions, "more than 128 actions is rejected");

    check(restored.action_count() == 2u && removable.action_count() == 0u,
          "bounded action count reflects load and precise removal");

    ActionMap every_key;
    for (int index = 1; index < static_cast<int>(Key::Count); ++index)
        check(
            every_key.bind("Key" + std::to_string(index),
                static_cast<Key>(index)),
            "all recognized key enum values can be bound");
    std::string every_key_serialized;
    ActionMap every_key_restored;
    check(
        every_key.serialize(every_key_serialized, &persistence_error) &&
        ActionMap::deserialize(
            every_key_serialized, every_key_restored, &persistence_error),
        "all keyboard and mouse key names round-trip");
    for (int index = 1; index < static_cast<int>(Key::Count); ++index) {
        const auto* mappings =
            every_key_restored.bindings("Key" + std::to_string(index));
        check(
            mappings && mappings->size() == 1 &&
            mappings->front().key == static_cast<Key>(index),
            "every serialized key maps back to its original enum value");
    }

    ActionMap invalid_for_export;
    invalid_for_export.bind("Invalid Action", Key::A);
    std::string untouched = "existing export contents";
    persistence_error.clear();
    check(
        !invalid_for_export.serialize(untouched, &persistence_error) &&
        untouched == "existing export contents" &&
        !persistence_error.empty(),
        "invalid action map export does not overwrite caller data");
    ActionMap excessive_export;
    for (char key = 'A'; key <= 'Q'; ++key)
        excessive_export.bind("Jump", static_cast<Key>(
            static_cast<int>(Key::A) + key - 'A'));
    check(
        !excessive_export.serialize(untouched) &&
        untouched == "existing export contents",
        "oversized action binding lists cannot be exported");

    if (failures != 0) {
        std::cerr << "NEngineInputTests: "
                  << failures
                  << " failure(s)\n";
        return 1;
    }

    std::cout << "NEngineInputTests: all checks passed\n";
    return 0;
}
