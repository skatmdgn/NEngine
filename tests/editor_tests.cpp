#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "nengine/core/scene.hpp"
#include "nengine/editor/editor_model.hpp"
#include "nengine/editor/presentation.hpp"
#include "nengine/editor/property_command.hpp"
#include "nengine/editor/scene_interaction.hpp"

namespace {

struct TestHealth {
    std::int64_t value{100};
};

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

    check(
        std::filesystem::exists(
            project_root / "NEngine.nproject"),
        "project creates persistent manifest");

    check(
        std::filesystem::exists(
            project_root /
            "Packages" /
            "managed-packages.txt"),
        "project creates managed NuGet package manifest template");

    check(
        model.project().manifest().startup_scene ==
            std::filesystem::path{
                "Assets/Scenes/Main.nscene"},
        "project manifest provides default startup scene");

    check(
        model.project().manifest().target_windows &&
        model.project().manifest().target_android,
        "project manifest enables agreed Windows and Android targets");

    model.layout().hierarchy_width = 310;
    model.layout().inspector_width = 360;
    model.layout().bottom_height = 190;

    const auto layout_path =
        project_root /
        "ProjectSettings" /
        "EditorLayout.layout";

    std::string layout_error;

    check(
        editor::EditorLayoutSerializer::save(
            model.layout(),
            layout_path,
            &layout_error),
        "editor layout saves");

    editor::EditorLayoutState loaded_layout;

    check(
        editor::EditorLayoutSerializer::load(
            layout_path,
            loaded_layout,
            &layout_error),
        "editor layout loads");

    check(
        loaded_layout.hierarchy_width == 310 &&
        loaded_layout.inspector_width == 360 &&
        loaded_layout.bottom_height == 190,
        "editor layout roundtrip preserves pane sizes");

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

    const auto first_import =
        model.project().import_asset(
            script_asset->guid);

    check(
        first_import.success &&
        !first_import.cache_hit,
        "project imports script into Library cache");

    check(
        !first_import.artifacts.empty() &&
        std::filesystem::exists(
            first_import.artifacts[0].path),
        "project import artifact exists");

    const auto second_import =
        model.project().import_asset(
            script_asset->guid);

    check(
        second_import.success &&
        second_import.cache_hit,
        "project script import reuses cache");

    auto& world = model.world();
    const auto root = world.create("Root");
    const auto child = world.create("Child");
    world.set_parent(child, root);
    model.selection().set(child);

    const auto health_type =
        core::ComponentRegistry::stable_id(
            "Tests.Health");

    check(
        model.component_registry().register_type(
            "Tests.Health",
            "Tests",
            false,
            false),
        "custom component descriptor registers");

    check(
        model.component_registry().register_property(
            health_type,
            {
                "Value",
                core::PropertyKind::Integer,
                core::PropertyFlags::Serializable |
                    core::PropertyFlags::Editable
            }),
        "custom component property metadata registers");

    auto* health =
        world.add_component<TestHealth>(
            child,
            health_type);

    check(
        health != nullptr,
        "custom component attaches to world entity");

    check(
        model.property_access().register_property(
            health_type,
            "Value",
            core::PropertyKind::Integer,
            [health_type](
                const core::World& world_value,
                core::Entity entity)
                -> std::optional<core::PropertyValue> {

                const auto* component =
                    world_value.get_component<TestHealth>(
                        entity,
                        health_type);

                if (!component) return std::nullopt;

                return core::PropertyValue{
                    component->value
                };
            },
            [health_type](
                core::World& world_value,
                core::Entity entity,
                const core::PropertyValue& value) {

                auto* component =
                    world_value.get_component<TestHealth>(
                        entity,
                        health_type);

                const auto* typed =
                    std::get_if<std::int64_t>(
                        &value);

                if (!component || !typed) {
                    return false;
                }

                component->value = *typed;
                return true;
            }),
        "custom property accessor registers");

    check(
        model.component_serialization().register_codec({
            health_type,
            1,
            "Tests.Health",
            [health_type](
                const core::World& source,
                core::Entity entity)
                -> std::optional<
                    core::SerializedComponentData> {

                const auto* component =
                    source.get_component<TestHealth>(
                        entity,
                        health_type);

                if (!component) {
                    return std::nullopt;
                }

                core::SerializedComponentData data;
                data.type = health_type;
                data.version = 1;
                data.type_name = "Tests.Health";
                data.properties.push_back({
                    "Value",
                    core::PropertyKind::Integer,
                    core::PropertyValue{
                        component->value
                    }
                });
                return data;
            },
            [health_type](
                core::World& destination,
                core::Entity entity,
                const core::SerializedComponentData& data,
                std::string* error) {

                std::int64_t value = 100;
                bool found = false;

                for (const auto& property :
                     data.properties) {

                    if (property.name != "Value") {
                        continue;
                    }

                    const auto* typed =
                        std::get_if<std::int64_t>(
                            &property.value);

                    if (!typed) {
                        if (error) {
                            *error =
                                "Health.Value has wrong type";
                        }
                        return false;
                    }

                    value = *typed;
                    found = true;
                }

                if (!found) {
                    if (error) {
                        *error =
                            "Health.Value missing";
                    }
                    return false;
                }

                auto* component =
                    destination.get_component<
                        TestHealth>(
                            entity,
                            health_type);

                if (!component) {
                    component =
                        destination.add_component<
                            TestHealth>(
                                entity,
                                health_type);
                }

                if (!component) {
                    if (error) {
                        *error =
                            "could not create Health component";
                    }
                    return false;
                }

                component->value = value;
                return true;
            }
        }),
        "custom component serialization codec registers");

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
    check(
        inspector.components.size() == 2,
        "generic inspector exposes Transform and custom component");

    bool saw_transform = false;
    bool saw_health = false;

    for (const auto& component :
         inspector.components) {

        if (component.type ==
            core::World::transform_type) {

            saw_transform = true;

            check(
                component.fields.size() == 3,
                "transform reflection produces inspector fields");

            check(
                std::get<core::Vec3>(
                    component.fields[0].value) ==
                    core::Vec3{4.0f, 5.0f, 6.0f},
                "generic inspector reads Transform through property adapter");
        }

        if (component.type == health_type) {
            saw_health = true;

            check(
                component.fields.size() == 1,
                "custom component reflection produces inspector field");

            check(
                std::get<std::int64_t>(
                    component.fields[0].value) == 100,
                "generic inspector reads arbitrary native component");
        }
    }

    check(
        saw_transform && saw_health,
        "generic inspector enumerates registered component types");

    check(
        model.commands().execute(
            world,
            std::make_unique<
                editor::SetPropertyCommand>(
                    &model.property_access(),
                    child,
                    health_type,
                    "Value",
                    core::PropertyValue{
                        std::int64_t{55}
                    })),
        "generic property command executes");

    check(
        world.get_component<TestHealth>(
            child,
            health_type)->value == 55,
        "generic property command writes custom component");

    check(
        model.commands().undo(world),
        "generic property command undo succeeds");

    check(
        world.get_component<TestHealth>(
            child,
            health_type)->value == 100,
        "generic property command undo restores custom component");

    check(
        model.commands().redo(world),
        "generic property command redo succeeds");

    check(
        world.get_component<TestHealth>(
            child,
            health_type)->value == 55,
        "generic property command redo reapplies custom component");

    const auto component_scene =
        core::SceneSerializer::capture(
            world,
            "ComponentRoundTrip",
            &model.component_serialization());

    std::stringstream component_stream;
    std::string component_scene_error;

    check(
        core::SceneSerializer::write(
            component_scene,
            component_stream,
            &component_scene_error),
        "Scene v2 serializes registered native component");

    core::SceneData component_loaded;

    check(
        core::SceneSerializer::read(
            component_stream,
            component_loaded,
            &component_scene_error),
        "Scene v2 parses registered native component");

    core::World component_restored;

    check(
        core::SceneSerializer::instantiate(
            component_loaded,
            component_restored,
            &component_scene_error,
            &model.component_serialization()),
        "Scene v2 restores registered native component");

    core::Entity restored_health_entity =
        core::Entity::invalid();

    for (const auto entity :
         component_restored.entities()) {
        if (component_restored.name(entity) ==
            "Renamed") {
            restored_health_entity = entity;
            break;
        }
    }

    check(
        restored_health_entity.valid(),
        "Scene v2 restored custom component owner");

    const auto* restored_health =
        component_restored.get_component<
            TestHealth>(
                restored_health_entity,
                health_type);

    check(
        restored_health &&
        restored_health->value == 55,
        "Scene v2 preserves custom component property value");

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

    {
        auto create_command =
            std::make_unique<
                editor::CreateEntityCommand>(
                    "Created",
                    root);

        auto* create_command_ptr =
            create_command.get();

        check(
            model.commands().execute(
                world,
                std::move(create_command)),
            "create entity command executes");

        const auto created =
            create_command_ptr->created_entity();

        check(
            world.is_alive(created) &&
            world.name(created) == "Created",
            "create entity command creates object");

        check(
            world.transform(created)->parent ==
                root,
            "create entity command assigns parent");

        check(
            model.commands().undo(world),
            "create entity undo succeeds");

        check(
            !world.is_alive(created),
            "create entity undo removes object");

        check(
            model.commands().redo(world),
            "create entity redo succeeds");

        const auto recreated =
            create_command_ptr->created_entity();

        check(
            world.is_alive(recreated),
            "create entity redo recreates object");
    }

    const auto delete_root =
        world.create("DeleteRoot");

    const auto delete_child =
        world.create("DeleteChild");

    world.set_parent(
        delete_child,
        delete_root);

    auto* delete_health =
        world.add_component<TestHealth>(
            delete_child,
            health_type);

    if (delete_health) {
        delete_health->value = 777;
    }

    check(
        model.commands().execute(
            world,
            std::make_unique<
                editor::DeleteEntityCommand>(
                    delete_root)),
        "delete entity command executes");

    check(
        !world.is_alive(delete_root) &&
        !world.is_alive(delete_child),
        "delete entity command removes subtree");

    check(
        model.commands().undo(world),
        "delete entity undo succeeds");

    check(
        world.is_alive(delete_root) &&
        world.is_alive(delete_child),
        "delete entity undo restores subtree");

    const auto* restored_delete_health =
        world.get_component<TestHealth>(
            delete_child,
            health_type);

    check(
        restored_delete_health &&
        restored_delete_health->value == 777,
        "delete entity undo restores component pools");

    check(
        model.commands().redo(world),
        "delete entity redo succeeds");

    check(
        !world.is_alive(delete_root) &&
        !world.is_alive(delete_child),
        "delete entity redo removes subtree again");

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
