#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include "nengine/editor/editor_model.hpp"
#include "nengine/editor/presentation.hpp"
#include "nengine/editor/scene_interaction.hpp"

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

    model.console().info("Test", "hello");
    model.console().info("Test", "hello");
    model.console().warning("Test", "warning");
    check(model.console().size() == 3, "console keeps non-consecutive startup/test entries");
    check(model.console().entries()[1].repeat_count == 2, "console collapses consecutive identical entries");
    check(model.console().count(editor::LogSeverity::Warning) == 1, "console counts severity");

    const auto project_stamp =
        std::chrono::high_resolution_clock::now()
            .time_since_epoch().count();

    const auto project_root =
        std::filesystem::temp_directory_path() /
        ("nengine_editor_project_" +
         std::to_string(project_stamp));

    std::string project_error;
    check(
        model.project().open(project_root, &project_error),
        "project session opens");
    check(
        std::filesystem::exists(project_root / "Assets" / "Scenes"),
        "project creates Assets/Scenes");
    check(
        std::filesystem::exists(project_root / "Library" / "Cache"),
        "project creates Library/Cache");

    {
        std::ofstream asset(
            project_root / "Assets" / "Scripts" / "Player.cs",
            std::ios::binary | std::ios::trunc);
        asset << "class Player {}";
    }

    const auto project_scan =
        model.project().refresh_assets();

    check(
        model.project().assets().find_relative("Scripts/Player.cs") != nullptr,
        "project asset database sees created script");

    const auto* script_asset =
        model.project().assets().find_relative("Scripts/Player.cs");

    check(
        script_asset &&
        script_asset->importer_id == "NEngine.Script",
        "project chooses script importer");

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

    const auto root_screen =
        editor::scene_entity_to_screen(
            world,
            root,
            800.0f,
            600.0f);

    check(
        root_screen.x == 400.0f &&
        root_screen.y == 300.0f,
        "scene projection centers root at origin");

    const auto child_screen =
        editor::scene_entity_to_screen(
            world,
            child,
            800.0f,
            600.0f);

    check(
        child_screen.x == 500.0f &&
        child_screen.y == 150.0f,
        "scene projection includes parent and child positions");

    check(
        editor::pick_scene_entity(
            world,
            501.0f,
            151.0f,
            800.0f,
            600.0f) == child,
        "scene picking selects nearest projected entity");

    check(
        editor::hit_test_translate_gizmo(
            world,
            child,
            child_screen.x + 30.0f,
            child_screen.y,
            800.0f,
            600.0f) ==
            editor::SceneGizmoAxis::X,
        "scene gizmo detects X axis");

    check(
        editor::hit_test_translate_gizmo(
            world,
            child,
            child_screen.x,
            child_screen.y - 30.0f,
            800.0f,
            600.0f) ==
            editor::SceneGizmoAxis::Z,
        "scene gizmo detects Z axis");

    const auto dragged_x =
        editor::translated_local_position_from_drag(
            world.transform(child)->local_position,
            editor::SceneGizmoAxis::X,
            50.0f,
            0.0f);

    check(
        dragged_x ==
            core::Vec3{6.0f, 5.0f, 6.0f},
        "X gizmo drag converts screen delta to local position");

    const auto dragged_z =
        editor::translated_local_position_from_drag(
            world.transform(child)->local_position,
            editor::SceneGizmoAxis::Z,
            0.0f,
            -50.0f);

    check(
        dragged_z ==
            core::Vec3{4.0f, 5.0f, 8.0f},
        "Z gizmo drag converts upward screen motion to positive Z");

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

    const auto runtime_inspector = editor::build_inspector(model);
    check(runtime_inspector.name == "Runtime Only", "inspector presents runtime world while playing");

    check(model.play_session().pause(), "play session pauses");
    toolbar = editor::build_toolbar(model);
    check(toolbar.can_resume && toolbar.can_step, "toolbar exposes resume and step while paused");
    check(model.play_session().step(), "paused session accepts a step request");
    check(model.play_session().requested_steps() == 1, "step request is counted");
    check(model.play_session().resume(), "play session resumes");
    check(model.play_session().stop(), "play session stops");
    check(model.play_session().runtime_world() == nullptr, "runtime world discarded on stop");
    check(model.can_edit(), "editing re-enabled after stop");

    const auto restored_inspector = editor::build_inspector(model);
    check(restored_inspector.name == "Renamed", "stopping play returns inspector to edit world");

    world.destroy(child);
    model.sanitize_selection();
    check(model.selection().empty(), "selection drops destroyed entities");

    model.project().close();
    std::error_code cleanup_error;
    std::filesystem::remove_all(
        project_root,
        cleanup_error);

    if (failures == 0) {
        std::cout << "NEngineEditorTests: PASS\n";
        return EXIT_SUCCESS;
    }
    std::cerr << "NEngineEditorTests: " << failures << " failure(s)\n";
    return EXIT_FAILURE;
}
