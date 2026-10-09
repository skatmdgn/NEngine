#include <algorithm>
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
#include "nengine/editor/property_text.hpp"
#include "nengine/editor/scene_interaction.hpp"
#include "nengine/render/builtin_assets.hpp"
#include "nengine/render/components.hpp"
#include "nengine/scripting/components.hpp"

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

    model.mark_scene_saved();
    check(
        !model.scene_dirty(),
        "fresh editor savepoint is clean");

    {
        core::PropertyValue parsed;
        std::string parse_error;

        check(
            editor::parse_property_value(
                core::PropertyKind::Boolean,
                "true",
                parsed,
                &parse_error) &&
            std::get<bool>(parsed),
            "generic property parser reads boolean");

        check(
            editor::parse_property_value(
                core::PropertyKind::Integer,
                "-42",
                parsed,
                &parse_error) &&
            std::get<std::int64_t>(parsed) == -42,
            "generic property parser reads signed integer");

        check(
            editor::parse_property_value(
                core::PropertyKind::Float,
                "3.25",
                parsed,
                &parse_error) &&
            std::get<double>(parsed) == 3.25,
            "generic property parser reads float");

        check(
            editor::parse_property_value(
                core::PropertyKind::Vec3,
                "1, 2, 3",
                parsed,
                &parse_error) &&
            std::get<core::Vec3>(parsed) ==
                core::Vec3{1.0f, 2.0f, 3.0f},
            "generic property parser reads Vec3");

        check(
            editor::parse_property_value(
                core::PropertyKind::Quaternion,
                "0 0 0 1",
                parsed,
                &parse_error) &&
            std::get<core::Quat>(parsed) ==
                core::Quat{0.0f, 0.0f, 0.0f, 1.0f},
            "generic property parser reads Quaternion");

        check(
            editor::parse_property_value(
                core::PropertyKind::EntityReference,
                "none",
                parsed,
                &parse_error) &&
            !std::get<core::Entity>(parsed).valid(),
            "generic property parser reads null EntityReference");

        check(
            !editor::parse_property_value(
                core::PropertyKind::Vec3,
                "1 2",
                parsed,
                &parse_error),
            "generic property parser rejects malformed Vec3");
    }

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
        std::filesystem::exists(project_root / "Assets" / "Materials"),
        "project creates Assets/Materials");
    check(
        std::filesystem::exists(project_root / "Assets" / "Animations"),
        "project creates Assets/Animations");
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


    const auto generated_model_path =
        project_root /
        "Assets" /
        "Models" /
        "GeneratedMaterial.gltf";

    std::filesystem::create_directories(
        generated_model_path
            .parent_path());

    {
        std::ofstream output(
            generated_model_path,
            std::ios::binary |
                std::ios::trunc);

        output
            << R"json({"asset":{"version":"2.0"},"meshes":[{"primitives":[{"material":0}]}],"materials":[{"pbrMetallicRoughness":{"baseColorFactor":[1,0,0,1]}}]})json";
    }

    model.project().refresh_assets();

    const auto* generated_model_record =
        model.project()
            .assets()
            .find_relative(
                "Models/GeneratedMaterial.gltf");

    check(
        generated_model_record != nullptr,
        "project asset database discovers glTF used for generated subasset test");

    assets::AssetGuid
        generated_material_guid{};

    assets::AssetGuid
        generated_texture_guid{};

    if (generated_model_record) {
        generated_material_guid =
            assets::derive_subasset_guid(
                generated_model_record->guid,
                "gltf-material",
                0u);

        generated_texture_guid =
            assets::derive_subasset_guid(
                generated_model_record->guid,
                "gltf-base-color",
                0u);

        const auto imported =
            model.project()
                .import_asset(
                    generated_model_record->guid);

        check(
            imported.success &&
            imported.subassets.size() == 3u,
            "ProjectSession model import indexes generated PBR material base-color and metallic-roughness subassets");

        const auto material_cache =
            model.project()
                .cached_artifacts(
                    generated_material_guid);

        const auto texture_cache =
            model.project()
                .cached_artifacts(
                    generated_texture_guid);

        check(
            material_cache &&
            material_cache->importer_id ==
                "NEngine.Material" &&
            texture_cache &&
            texture_cache->importer_id ==
                "NEngine.Texture",
            "ProjectSession resolves generated subasset GUIDs through normal cached_artifacts API");
    }

    model.project().close();

    check(
        model.project().open(
            project_root,
            &project_error),
        "project session reopens after generated subasset import");

    const auto reopened_material_cache =
        model.project()
            .cached_artifacts(
                generated_material_guid);

    const auto reopened_texture_cache =
        model.project()
            .cached_artifacts(
                generated_texture_guid);

    check(
        reopened_material_cache &&
        reopened_material_cache
            ->importer_id ==
            "NEngine.Material" &&
        reopened_texture_cache &&
        reopened_texture_cache
            ->importer_id ==
            "NEngine.Texture",
        "ProjectSession lazily recovers generated subasset GUIDs from persistent parent import manifest after reopen");

    check(
        model.project().manifest().startup_scene ==
            std::filesystem::path{
                "Assets/Scenes/Main.nscene"},
        "project manifest provides default startup scene");

    check(
        std::filesystem::exists(
            model.project().startup_scene_path()),
        "new project creates startup scene file");

    core::SceneData startup_scene_data;
    std::string startup_scene_error;

    check(
        core::SceneSerializer::load_file(
            model.project().startup_scene_path(),
            startup_scene_data,
            &startup_scene_error),
        "project startup scene parses");

    check(
        startup_scene_data.objects.size() == 4,
        "default startup scene contains camera light and demo objects");

    core::World startup_world;

    check(
        core::SceneSerializer::instantiate(
            startup_scene_data,
            startup_world,
            &startup_scene_error,
            &model.component_serialization()),
        "default startup scene instantiates with engine components");

    core::Entity startup_camera =
        core::Entity::invalid();

    core::Entity startup_light =
        core::Entity::invalid();

    core::Entity startup_cube =
        core::Entity::invalid();

    core::Entity startup_child_cube =
        core::Entity::invalid();

    for (const auto entity :
         startup_world.entities()) {
        if (startup_world.name(entity) ==
            "Main Camera") {
            startup_camera = entity;
        }

        if (startup_world.name(entity) ==
            "Directional Light") {
            startup_light = entity;
        }

        if (startup_world.name(entity) ==
            "Cube") {
            startup_cube = entity;
        }

        if (startup_world.name(entity) ==
            "Child Cube") {
            startup_child_cube = entity;
        }
    }

    check(
        startup_camera.valid() &&
        startup_world.get_component<
            render::Camera>(
                startup_camera,
                render::camera_type()) != nullptr,
        "startup Main Camera owns native Camera component");

    check(
        startup_light.valid() &&
        startup_world.get_component<
            render::Light>(
                startup_light,
                render::light_type()) != nullptr,
        "startup Directional Light owns native Light component");

    const auto* startup_cube_renderer =
        startup_cube.valid()
            ? startup_world.get_component<
                render::MeshRenderer>(
                    startup_cube,
                    render::mesh_renderer_type())
            : nullptr;

    const auto* startup_child_renderer =
        startup_child_cube.valid()
            ? startup_world.get_component<
                render::MeshRenderer>(
                    startup_child_cube,
                    render::mesh_renderer_type())
            : nullptr;

    check(
        startup_cube_renderer &&
        startup_cube_renderer->mesh ==
            render::
                builtin_unit_cube_mesh_guid(),
        "startup Cube owns MeshRenderer referencing built-in unit cube");

    check(
        startup_child_renderer &&
        startup_child_renderer->mesh ==
            render::
                builtin_unit_cube_mesh_guid(),
        "startup Child Cube owns MeshRenderer referencing built-in unit cube");

    check(
        model.project().manifest().target_windows &&
        model.project().manifest().target_android,
        "project manifest enables agreed Windows and Android targets");

    {
        auto invalid_manifest =
            model.project().manifest();

        invalid_manifest.startup_scene =
            "../Outside.nscene";

        std::string manifest_error;

        check(
            !editor::ProjectManifestSerializer::validate(
                invalid_manifest,
                &manifest_error),
            "project manifest rejects startup-scene path traversal");
    }

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

    const auto script_activation =
        model.project().activation_for(
            script_asset->guid);

    check(
        script_activation.kind ==
            editor::AssetActivationKind::OpenScript &&
        script_activation.source_path ==
            script_asset->source_path,
        "script asset routes to external script editor");

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

    // Consume the watcher event for Player.cs so the next poll
    // observes only the newly created texture.
    const auto script_poll =
        model.project().poll_assets();

    check(
        !script_poll.changes.empty(),
        "project watcher observes previously created script");

    const auto texture_path =
        project_root /
        "Assets" /
        "Textures" /
        "checker.png";

    std::filesystem::create_directories(
        texture_path.parent_path());

    {
        std::ofstream texture(
            texture_path,
            std::ios::binary |
                std::ios::trunc);

        texture
            << "\x89PNG\r\n\x1A\n"
            << "NEngineTextureTest";
    }

    const auto texture_poll =
        model.project().poll_assets();

    check(
        texture_poll.changes.size() == 1 &&
        texture_poll.changes[0].kind ==
            assets::FileChangeKind::Added,
        "project watcher isolates newly added texture");

    check(
        texture_poll.imports.attempted == 1 &&
        texture_poll.imports.imported == 1 &&
        texture_poll.imports.failed == 0,
        "new texture is automatically imported");

    const auto* texture_asset =
        model.project().assets().find_relative(
            "Textures/checker.png");

    check(
        texture_asset &&
        texture_asset->importer_id ==
            "NEngine.Texture",
        "texture asset selects built-in texture importer");

    check(
        texture_asset &&
        model.project().activation_for(
            texture_asset->guid).kind ==
            editor::AssetActivationKind::OpenExternal,
        "texture asset routes to external preview fallback");

    const auto texture_guid =
        texture_asset
            ? texture_asset->guid
            : assets::AssetGuid{};

    const auto material_path =
        project_root /
        "Assets" /
        "Materials" /
        "Checker.nmat";

    {
        std::ofstream material(
            material_path,
            std::ios::binary |
                std::ios::trunc);

        material
            << "NENGINE_MATERIAL 1\n"
            << "BASE_COLOR_TEXTURE \""
            << texture_guid.to_string()
            << "\"\n"
            << "END_MATERIAL\n";
    }

    const auto material_poll =
        model.project().poll_assets();

    check(
        material_poll.changes.size() == 1 &&
        material_poll.imports.attempted == 1 &&
        material_poll.imports.imported == 1 &&
        material_poll.imports.failed == 0,
        "new nmat material is detected and automatically imported");

    const auto* material_asset =
        model.project().assets().find_relative(
            "Materials/Checker.nmat");

    check(
        material_asset &&
        material_asset->importer_id ==
            "NEngine.Material",
        "nmat asset selects material importer");

    if (material_asset) {
        const auto dependencies =
            model.project()
                .dependency_graph()
                .dependencies(
                    material_asset->guid);

        check(
            dependencies.size() == 1u &&
            dependencies[0] ==
                texture_guid,
            "material importer records base-color texture dependency");
    }

    const auto persistent_material_guid =
        material_asset ? material_asset->guid : assets::AssetGuid{};

    const auto animation_path =
        project_root /
        "Assets" /
        "Animations" /
        "Pulse.nspriteanim";

    {
        std::ofstream animation(
            animation_path,
            std::ios::binary |
                std::ios::trunc);

        animation
            << "NENGINE_SPRITE_ANIMATION 1\n"
            << "FRAMES 2\n"
            << "FRAME \""
            << texture_guid.to_string()
            << "\" 0.05\n"
            << "FRAME \""
            << texture_guid.to_string()
            << "\" 0.10\n"
            << "END_SPRITE_ANIMATION\n";
    }

    const auto animation_poll =
        model.project().poll_assets();

    check(
        animation_poll.changes.size() == 1u &&
        animation_poll.imports.attempted == 1u &&
        animation_poll.imports.imported == 1u &&
        animation_poll.imports.failed == 0u,
        "new nspriteanim is detected and automatically imported");

    const auto* animation_asset =
        model.project().assets().find_relative(
            "Animations/Pulse.nspriteanim");

    check(
        animation_asset &&
        animation_asset->importer_id ==
            "NEngine.SpriteAnimation",
        "nspriteanim selects SpriteAnimation importer");

    if (animation_asset) {
        const auto dependencies =
            model.project()
                .dependency_graph()
                .dependencies(
                    animation_asset->guid);

        check(
            dependencies.size() == 1u &&
            dependencies.front() ==
                texture_guid,
            "SpriteAnimation importer de-duplicates and records frame texture dependency");

        const auto cached_animation =
            model.project().import_asset(
                animation_asset->guid);

        const auto cached_dependencies =
            model.project()
                .dependency_graph()
                .dependencies(
                    animation_asset->guid);

        check(
            cached_animation.success &&
            cached_animation.cache_hit &&
            cached_dependencies.size() == 1u &&
            cached_dependencies.front() ==
                texture_guid,
            "SpriteAnimation dependency survives import cache hit");
    }

    // External glTF sources should stage .bin/images as ordinary asset
    // dependencies. A sidecar edit invalidates the parent model import
    // even though the .gltf text and size are unchanged.
    const auto models_root =
        project_root / "Assets" / "Models";
    const auto sidecar_root =
        models_root / "SidecarTest";
    const auto geometry_path =
        sidecar_root / "geometry" / "mesh.bin";
    const auto image_path =
        sidecar_root / "images" / "albedo.png";
    const auto gltf_path =
        sidecar_root / "scene.gltf";

    std::filesystem::create_directories(geometry_path.parent_path());
    std::filesystem::create_directories(image_path.parent_path());

    {
        std::ofstream output(
            geometry_path, std::ios::binary | std::ios::trunc);
        output << "12345678";
    }
    {
        std::ofstream output(
            image_path, std::ios::binary | std::ios::trunc);
        output << "\x89PNG\r\n\x1A\n";
    }
    {
        std::ofstream output(
            gltf_path, std::ios::binary | std::ios::trunc);
        output
            << R"json({"asset":{"version":"2.0"},"buffers":[{"uri":"geometry/mesh.bin","byteLength":8}],"images":[{"uri":"images/albedo.png"}]})json";
    }

    const auto model_poll =
        model.project().poll_assets();
    const auto* imported_gltf =
        model.project().assets().find_relative(
            "Models/SidecarTest/scene.gltf");
    const auto* imported_bin =
        model.project().assets().find_relative(
            "Models/SidecarTest/geometry/mesh.bin");
    const auto* imported_image =
        model.project().assets().find_relative(
            "Models/SidecarTest/images/albedo.png");

    check(
        model_poll.imports.failed == 0 &&
        imported_gltf && imported_bin && imported_image,
        "project imports glTF alongside external bin/image sidecars");

    const auto model_guid =
        imported_gltf ? imported_gltf->guid : assets::AssetGuid{};
    const auto bin_guid =
        imported_bin ? imported_bin->guid : assets::AssetGuid{};
    const auto image_guid =
        imported_image ? imported_image->guid : assets::AssetGuid{};

    if (model_guid.valid()) {
        const auto dependencies =
            model.project().dependency_graph().dependencies(model_guid);

        check(
            dependencies.size() == 2u &&
            std::find(dependencies.begin(), dependencies.end(), bin_guid) !=
                dependencies.end() &&
            std::find(dependencies.begin(), dependencies.end(), image_guid) !=
                dependencies.end(),
            "glTF import records sidecar AssetGuid dependencies");
    }

    const auto staged_model =
        model.project().cached_artifacts(model_guid);
    std::string original_model_fingerprint;
    std::size_t sidecar_artifacts = 0;

    if (staged_model) {
        original_model_fingerprint = staged_model->fingerprint;
        for (const auto& artifact : staged_model->artifacts) {
            if (artifact.role == "model-sidecar" &&
                std::filesystem::is_regular_file(artifact.path)) {
                ++sidecar_artifacts;
            }
        }
    }

    check(
        staged_model.has_value() &&
        sidecar_artifacts == 2u &&
        std::filesystem::exists(
            project_root / "Library" / "Cache" /
            model_guid.to_string() / "geometry" / "mesh.bin") &&
        std::filesystem::exists(
            project_root / "Library" / "Cache" /
            model_guid.to_string() / "images" / "albedo.png"),
        "glTF importer stages relative binary and image paths for renderer");

    {
        std::ofstream output(
            geometry_path, std::ios::binary | std::ios::trunc);
        output << "87654321";
    }
    std::error_code sidecar_time_error;
    const auto old_time = std::filesystem::last_write_time(
        geometry_path, sidecar_time_error);
    if (!sidecar_time_error) {
        std::filesystem::last_write_time(
            geometry_path, old_time + std::chrono::seconds(2),
            sidecar_time_error);
    }

    check(
        !model.project().cached_artifacts(model_guid).has_value(),
        "edited glTF sidecar invalidates cached model fingerprint");

    const auto changed_sidecar_poll =
        model.project().poll_assets();

    const auto restaged_model =
        model.project().cached_artifacts(model_guid);

    check(
        changed_sidecar_poll.imports.attempted >= 2u &&
        changed_sidecar_poll.imports.failed == 0 &&
        restaged_model.has_value() &&
        restaged_model->fingerprint != original_model_fingerprint,
        "sidecar file watcher reimports dependent glTF model");

    {
        std::ifstream input(
            project_root / "Library" / "Cache" /
            model_guid.to_string() / "geometry" / "mesh.bin",
            std::ios::binary);
        std::string copied(8, '\0');
        input.read(copied.data(), 8);
        check(
            input.good() && copied == "87654321",
            "updated glTF sidecar payload is restaged into model cache");
    }


    std::error_code removed_sidecar_error;
    std::filesystem::remove(geometry_path, removed_sidecar_error);
    const auto missing_sidecar_poll = model.project().poll_assets();

    const auto waiting_dependents =
        model.project().dependency_graph().dependents(bin_guid);

    check(
        !removed_sidecar_error &&
        missing_sidecar_poll.imports.failed >= 1u &&
        !model.project().cached_artifacts(model_guid).has_value() &&
        std::find(
            waiting_dependents.begin(),
            waiting_dependents.end(),
            model_guid) != waiting_dependents.end(),
        "deleted glTF sidecar invalidates model but keeps its reverse dependency");

    {
        std::ofstream output(
            geometry_path, std::ios::binary | std::ios::trunc);
        output << "RESTORED";
    }

    const auto restored_sidecar_poll =
        model.project().poll_assets();
    const auto* restored_bin =
        model.project().assets().find_relative(
            "Models/SidecarTest/geometry/mesh.bin");

    check(
        restored_sidecar_poll.imports.failed == 0u &&
        restored_sidecar_poll.imports.attempted >= 2u &&
        restored_bin && restored_bin->guid == bin_guid &&
        model.project().cached_artifacts(model_guid).has_value(),
        "restoring glTF sidecar with preserved .meta automatically reimports parent model");

    // Cache hits must retain a separate .nmat's texture dependency.
    if (persistent_material_guid.valid()) {
        const auto cached_material =
            model.project().import_asset(persistent_material_guid);
        const auto dependencies =
            model.project().dependency_graph().dependencies(
                persistent_material_guid);

        check(
            cached_material.success && cached_material.cache_hit &&
            dependencies.size() == 1u &&
            dependencies.front() == texture_guid,
            "material dependency survives import cache hit");
    }

    auto& world = model.world();

    const auto render_entity =
        world.create("Render Camera");

    auto* render_camera =
        world.add_component<
            render::Camera>(
                render_entity,
                render::camera_type());

    check(
        render_camera != nullptr,
        "EditorModel accepts registered Camera component");

    model.selection().set(
        render_entity);

    const auto render_inspector =
        editor::build_inspector(
            model);

    bool saw_camera_component = false;

    for (const auto& component :
         render_inspector.components) {

        if (component.type !=
            render::camera_type()) {
            continue;
        }

        saw_camera_component = true;

        check(
            component.fields.size() == 6,
            "generic Inspector exposes Camera properties");
    }

    check(
        saw_camera_component,
        "generic Inspector includes Camera component");

    // Regression: the Win32 property list must retain the selected
    // (component type, property path) after a full Inspector rebuild,
    // even when the same property name occurs on different components.
    auto property_snapshot =
        render_inspector;

    editor::InspectorComponent mesh_component;
    mesh_component.type =
        render::mesh_renderer_type();
    mesh_component.name =
        "NEngine.MeshRenderer";
    mesh_component.fields.push_back({
        "Enabled",
        "Enabled",
        core::PropertyKind::Boolean,
        core::PropertyValue{true},
        true
    });
    property_snapshot.components.insert(
        property_snapshot.components.begin(),
        mesh_component);

    const auto enabled_row =
        editor::find_inspector_property_row(
            property_snapshot,
            render::mesh_renderer_type(),
            "Enabled");

    const auto fov_row =
        editor::find_inspector_property_row(
            property_snapshot,
            render::camera_type(),
            "Vertical FOV");

    check(
        enabled_row.has_value() &&
        *enabled_row == 0u &&
        fov_row.has_value() &&
        *fov_row != *enabled_row,
        "Reflection Properties distinguishes a selected field from first MeshRenderer.Enabled row");

    auto reordered_snapshot =
        property_snapshot;

    for (auto& component :
         reordered_snapshot.components) {
        if (component.type ==
            render::camera_type()) {
            std::reverse(
                component.fields.begin(),
                component.fields.end());
        }
    }

    const auto reordered_fov_row =
        editor::find_inspector_property_row(
            reordered_snapshot,
            render::camera_type(),
            "Vertical FOV");

    check(
        fov_row.has_value() &&
        reordered_fov_row.has_value() &&
        *fov_row != *reordered_fov_row,
        "Reflection Properties restores selection by field identity after row reorder");

    check(
        !editor::find_inspector_property_row(
            property_snapshot,
            render::camera_type(),
            "Unknown Field").has_value(),
        "Reflection Properties detects removed fields instead of selecting a stale row");

    check(
        model.commands().execute(
            world,
            std::make_unique<
                editor::SetPropertyCommand>(
                    &model.property_access(),
                    render_entity,
                    render::camera_type(),
                    "Vertical FOV",
                    core::PropertyValue{
                        80.0
                    })),
        "generic property command edits Camera FOV");

    check(
        render_camera &&
        render_camera->vertical_fov_degrees ==
            80.0f,
        "Camera FOV edit reaches native render component");

    check(
        model.commands().undo(
            world),
        "Camera property edit supports undo");

    check(
        render_camera &&
        render_camera->vertical_fov_degrees ==
            60.0f,
        "Camera FOV undo restores prior value");

    model.commands().clear();

    const auto factory_entity =
        world.create(
            "Factory Target");

    check(
        model.component_factories().contains(
            render::camera_type()) &&
        model.component_factories().contains(
            render::light_type()) &&
        model.component_factories().contains(
            render::mesh_renderer_type()) &&
        model.component_factories().contains(
            render::sprite_renderer_type()) &&
        model.component_factories().contains(
            render::sprite_animator_type()),
        "EditorModel registers add-component factories for all native render components");

    check(
        model.commands().execute(
            world,
            std::make_unique<
                editor::AddComponentCommand>(
                    factory_entity,
                    render::sprite_animator_type(),
                    &model.component_factories())) &&
        world.has_component(
            factory_entity,
            render::sprite_animator_type()),
        "AddComponentCommand creates SpriteAnimator through generic factory registry");

    check(
        !model.component_factories().add(
            world,
            factory_entity,
            render::sprite_animator_type()),
        "component factory rejects duplicate component on the same entity");

    check(
        model.commands().undo(
            world) &&
        !world.has_component(
            factory_entity,
            render::sprite_animator_type()),
        "AddComponentCommand undo removes the added component");

    check(
        model.commands().redo(
            world) &&
        world.has_component(
            factory_entity,
            render::sprite_animator_type()),
        "AddComponentCommand redo recreates the component");

    model.commands().clear();


    check(
        model.component_factories().contains(
            scripting::script_behaviour_type()),
        "EditorModel registers ScriptBehaviour Add Component factory");

    const auto script_entity =
        world.create(
            "Script Entity");

    check(
        model.commands().execute(
            world,
            std::make_unique<
                editor::AddComponentCommand>(
                    script_entity,
                    scripting::script_behaviour_type(),
                    &model.component_factories())),
        "AddComponentCommand creates ScriptBehaviour through generic factory registry");

    auto* script_behaviour =
        world.get_component<
            scripting::ScriptBehaviour>(
                script_entity,
                scripting::script_behaviour_type());

    check(
        script_behaviour != nullptr,
        "ScriptBehaviour native component exists after Add Component");

    check(
        model.commands().execute(
            world,
            std::make_unique<
                editor::SetPropertyCommand>(
                    &model.property_access(),
                    script_entity,
                    scripting::script_behaviour_type(),
                    "Type Name",
                    core::PropertyValue{
                        std::string{
                            "Game.PlayerController"}})),
        "generic property command edits ScriptBehaviour Type Name");

    check(
        script_behaviour &&
        script_behaviour->type_name ==
            "Game.PlayerController",
        "ScriptBehaviour Type Name edit reaches native scripting component");

    check(
        model.property_access().write(
            world,
            script_entity,
            scripting::script_behaviour_type(),
            "Enabled",
            core::PropertyValue{
                false}),
        "generic property access edits ScriptBehaviour Enabled");

    check(
        script_behaviour &&
        !script_behaviour->enabled,
        "ScriptBehaviour Enabled edit reaches native scripting component");

    model.commands().clear();

    check(
        world.destroy(
            script_entity),
        "ScriptBehaviour test entity cleans up before hierarchy assertions");

    check(
        world.destroy(
            factory_entity),
        "factory command test entity cleans up before hierarchy assertions");

    const auto sprite_entity =
        world.create("Render Sprite");

    auto* sprite_renderer =
        world.add_component<
            render::SpriteRenderer>(
                sprite_entity,
                render::sprite_renderer_type());

    check(
        sprite_renderer != nullptr,
        "EditorModel accepts registered SpriteRenderer component");

    const auto sprite_texture =
        assets::AssetGuid::generate();

    if (sprite_renderer) {
        sprite_renderer->texture =
            sprite_texture;
    }

    model.selection().set(
        sprite_entity);

    const auto sprite_inspector =
        editor::build_inspector(
            model);

    const auto sprite_component =
        std::find_if(
            sprite_inspector.components.begin(),
            sprite_inspector.components.end(),
            [](const auto& component) {
                return component.type ==
                    render::sprite_renderer_type();
            });

    check(
        sprite_component !=
            sprite_inspector.components.end() &&
        sprite_component->fields.size() == 6u,
        "generic Inspector exposes SpriteRenderer texture PPU sort and flip properties");

    check(
        model.commands().execute(
            world,
            std::make_unique<
                editor::SetPropertyCommand>(
                    &model.property_access(),
                    sprite_entity,
                    render::sprite_renderer_type(),
                    "Pixels Per Unit",
                    core::PropertyValue{
                        32.0
                    })),
        "generic property command edits SpriteRenderer Pixels Per Unit");

    check(
        sprite_renderer &&
        sprite_renderer->pixels_per_unit ==
            32.0f,
        "SpriteRenderer PPU edit reaches native render component");

    check(
        !model.property_access().write(
            world,
            sprite_entity,
            render::sprite_renderer_type(),
            "Pixels Per Unit",
            core::PropertyValue{
                0.0
            }),
        "SpriteRenderer generic property access rejects non-positive Pixels Per Unit");

    check(
        model.commands().undo(
            world) &&
        sprite_renderer &&
        sprite_renderer->pixels_per_unit ==
            100.0f,
        "SpriteRenderer PPU edit supports undo");

    model.commands().clear();

    check(
        world.destroy(sprite_entity),
        "sprite render integration test entity cleanup succeeds");

    check(
        world.destroy(render_entity),
        "render integration test entity cleanup succeeds");

    model.selection().clear();

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
    model.mark_scene_saved();

    check(model.commands().execute(world, std::make_unique<editor::RenameEntityCommand>(child, "Renamed")), "rename command executes");
    check(world.name(child) == "Renamed", "rename command changes world");
    check(model.scene_dirty(), "command after savepoint marks scene dirty");

    auto toolbar = editor::build_toolbar(model);
    check(toolbar.can_undo && toolbar.undo_label == "Undo Rename Entity", "toolbar reflects undo history");

    check(model.commands().undo(world), "undo succeeds");
    check(world.name(child) == "Child", "undo restores previous name");
    check(!model.scene_dirty(), "undo back to savepoint clears dirty state");
    check(model.commands().redo(world), "redo succeeds");
    check(model.scene_dirty(), "redo away from savepoint restores dirty state");
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


    core::Entity restored_script_entity =
        core::Entity::invalid();

    for (const auto entity :
         component_restored.entities()) {

        if (component_restored.name(entity) ==
            "Script Entity") {

            restored_script_entity =
                entity;
            break;
        }
    }

    const auto* restored_script =
        restored_script_entity.valid()
            ? component_restored
                .get_component<
                    scripting::ScriptBehaviour>(
                        restored_script_entity,
                        scripting::
                            script_behaviour_type())
            : nullptr;

    check(
        restored_script &&
        restored_script->type_name ==
            "Game.PlayerController" &&
        !restored_script->enabled,
        "Scene v2 roundtrip preserves ScriptBehaviour Type Name and Enabled");

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

    check(
        model.play_session().consume_simulation_steps(
            0.034) == 2u,
        "playing PlaySession converts host time into bounded 60 Hz fixed steps");

    const auto runtime_inspector = editor::build_inspector(model);
    check(runtime_inspector.name == "Runtime Only", "inspector presents runtime world while playing");

    check(model.play_session().pause(), "play session pauses");
    toolbar = editor::build_toolbar(model);
    check(toolbar.can_resume && toolbar.can_step, "toolbar exposes resume and step while paused");
    check(model.play_session().step(), "paused session accepts a step request");
    check(model.play_session().requested_steps() == 1, "step request is counted");
    check(
        model.play_session().consume_simulation_steps(
            0.0) == 1u &&
        model.play_session().requested_steps() == 0u,
        "paused PlaySession consumes Step as exactly one fixed simulation frame");
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
