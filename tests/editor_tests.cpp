#include <cstdlib>
#include <iostream>
#include <memory>

#include "nengine/editor/editor_model.hpp"

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
    using namespace nengine;

    editor::EditorModel model;
    auto& world = model.world();
    const auto object = world.create("Object");
    model.selection().set(object);

    check(model.selection().active() == object, "selection tracks active entity");
    check(model.commands().execute(world, std::make_unique<editor::RenameEntityCommand>(object, "Renamed")), "rename command executes");
    check(world.name(object) == "Renamed", "rename command changes world");
    check(model.commands().undo(world), "undo succeeds");
    check(world.name(object) == "Object", "undo restores previous name");
    check(model.commands().redo(world), "redo succeeds");
    check(world.name(object) == "Renamed", "redo reapplies name");

    core::Transform moved = *world.transform(object);
    moved.local_position = {4.0f, 5.0f, 6.0f};
    check(model.commands().execute(world, std::make_unique<editor::SetTransformCommand>(object, moved)), "transform command executes");
    check(world.transform(object)->local_position == core::Vec3{4.0f, 5.0f, 6.0f}, "transform command changes value");
    check(model.commands().undo(world), "transform undo succeeds");
    check(world.transform(object)->local_position == core::Vec3{}, "transform undo restores value");

    check(model.play_session().play(world), "play clones edit world");
    check(!model.can_edit(), "edit operations can be gated during play");
    auto* runtime = model.play_session().runtime_world();
    check(runtime && runtime->is_alive(object), "runtime clone preserves entity handles");
    runtime->set_name(object, "Runtime Only");
    check(world.name(object) == "Renamed", "runtime mutations do not alter editor world");
    check(model.play_session().pause(), "play session pauses");
    check(model.play_session().step(), "paused session accepts a step request");
    check(model.play_session().requested_steps() == 1, "step request is counted");
    check(model.play_session().resume(), "play session resumes");
    check(model.play_session().stop(), "play session stops");
    check(model.play_session().runtime_world() == nullptr, "runtime world discarded on stop");
    check(model.can_edit(), "editing re-enabled after stop");

    world.destroy(object);
    model.sanitize_selection();
    check(model.selection().empty(), "selection drops destroyed entities");

    if (failures == 0) {
        std::cout << "NEngineEditorTests: PASS\n";
        return EXIT_SUCCESS;
    }
    std::cerr << "NEngineEditorTests: " << failures << " failure(s)\n";
    return EXIT_FAILURE;
}
