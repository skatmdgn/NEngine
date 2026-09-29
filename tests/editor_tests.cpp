#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

#include "nengine/editor/editor_model.hpp"
#include "nengine/editor/presentation.hpp"

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
    const auto root = world.create("Root");
    const auto child = world.create("Child");
    world.set_parent(child, root);
    model.selection().set(child);

    check(model.selection().active() == child, "selection tracks active entity");
    check(model.commands().execute(world, std::make_unique<editor::RenameEntityCommand>(child, "Renamed")), "rename command executes");
    check(world.name(child) == "Renamed", "rename command changes world");

    auto toolbar = editor::build_toolbar(model);
    check(toolbar.can_undo && toolbar.undo_label == "Undo Rename Entity", "toolbar reflects undo history");

    check(model.commands().undo(world), "undo succeeds");
    check(world.name(child) == "Child", "undo restores previous name");
    check(model.commands().redo(world), "redo succeeds");
    check(world.name(child) == "Renamed", "redo reapplies name");

    check(model.commands().execute(world, std::make_unique<editor::SetActiveCommand>(child, false)), "active command executes");
    check(!world.active(child), "active command changes object state");
    check(model.commands().undo(world), "active undo succeeds");
    check(world.active(child), "active undo restores state");

    core::Transform moved = *world.transform(child);
    moved.local_position = {4.0f, 5.0f, 6.0f};
    check(model.commands().execute(world, std::make_unique<editor::SetTransformCommand>(child, moved)), "transform command executes");
    check(world.transform(child)->local_position == core::Vec3{4.0f, 5.0f, 6.0f}, "transform command changes value");

    const auto hierarchy = editor::build_hierarchy(model);
    check(hierarchy.size() == 2, "hierarchy view includes all world objects");
    check(hierarchy[0].entity == root && hierarchy[0].depth == 0, "hierarchy root row has depth zero");
    check(hierarchy[1].entity == child && hierarchy[1].depth == 1 && hierarchy[1].selected, "hierarchy child row has depth one and selection state");

    const auto inspector = editor::build_inspector(model);
    check(inspector.valid && inspector.entity == child, "inspector follows active selection");
    check(inspector.name == "Renamed", "inspector snapshots object metadata");
    check(inspector.components.size() == 1, "transform is exposed as inspector component");
    check(inspector.components[0].fields.size() == 3, "transform reflection produces inspector fields");
    check(std::get<core::Vec3>(inspector.components[0].fields[0].value) == core::Vec3{4.0f, 5.0f, 6.0f}, "inspector field reads live transform data");

    check(model.play_session().play(world), "play clones edit world");
    check(!model.can_edit(), "edit operations can be gated during play");
    toolbar = editor::build_toolbar(model);
    check(!toolbar.can_play && toolbar.can_stop && toolbar.can_pause && !toolbar.can_undo, "toolbar switches to playing state");

    auto* runtime = model.play_session().runtime_world();
    check(runtime && runtime->is_alive(child), "runtime clone preserves entity handles");
    runtime->set_name(child, "Runtime Only");
    check(world.name(child) == "Renamed", "runtime mutations do not alter editor world");
    check(model.play_session().pause(), "play session pauses");
    toolbar = editor::build_toolbar(model);
    check(toolbar.can_resume && toolbar.can_step, "toolbar exposes resume and step while paused");
    check(model.play_session().step(), "paused session accepts a step request");
    check(model.play_session().requested_steps() == 1, "step request is counted");
    check(model.play_session().resume(), "play session resumes");
    check(model.play_session().stop(), "play session stops");
    check(model.play_session().runtime_world() == nullptr, "runtime world discarded on stop");
    check(model.can_edit(), "editing re-enabled after stop");

    world.destroy(child);
    model.sanitize_selection();
    check(model.selection().empty(), "selection drops destroyed entities");

    if (failures == 0) {
        std::cout << "NEngineEditorTests: PASS\n";
        return EXIT_SUCCESS;
    }
    std::cerr << "NEngineEditorTests: " << failures << " failure(s)\n";
    return EXIT_FAILURE;
}
