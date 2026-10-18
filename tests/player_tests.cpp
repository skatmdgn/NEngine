#include <array>
#include <cstdint>
#include <cmath>
#include <limits>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include "nengine/player/bundle.hpp"
#include "nengine/player/exporter.hpp"
#include "nengine/player/runtime.hpp"
#include "nengine/player/subassets.hpp"
#include "nengine/input/action_map_io.hpp"
#include "nengine/player/managed_properties.hpp"
#include "nengine/core/scene.hpp"
#include "nengine/render/components.hpp"
#include "nengine/render/model_importer.hpp"
#include "nengine/ui/canvas_serialization.hpp"
#include "nengine/render/material_asset.hpp"
#include "nengine/render/asset_resources.hpp"
#include "nengine/render/decoded_texture.hpp"
#include "nengine/render/registration.hpp"
#include "nengine/audio/components.hpp"
#include "nengine/audio/registration.hpp"
#include "nengine/physics/components.hpp"
#include "nengine/physics/collision.hpp"
#include "nengine/physics/registration.hpp"
#include "nengine/scripting/components.hpp"
#include "nengine/scripting/registration.hpp"

static int test_failure(int code, int line) {
    std::cerr << "FAIL: NEnginePlayerTests check " << code
              << " at line " << line << '\n';
    return code;
}

int main() {
    using namespace nengine::player;
    BundleManifest manifest;
    manifest.title = "Demo";
    std::stringstream stream;
    if (!write_bundle(stream, manifest)) return test_failure(1, __LINE__);
    BundleManifest restored;
    if (!read_bundle(stream, restored) ||
        restored.title != manifest.title ||
        restored.scene != manifest.scene) return test_failure(2, __LINE__);
    restored.scene = "../outside.nscene";
    if (validate_bundle(restored)) return test_failure(3, __LINE__);
    restored = manifest;
    restored.width = 0;
    if (validate_bundle(restored)) return test_failure(4, __LINE__);
    const auto root = std::filesystem::temp_directory_path() /
        "nengine_player_bundle_test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "Content");
    { std::ofstream file(root / "Content" / "Main.nscene"); file << "fixture"; }
    std::string error;
    if (!resolve_bundle_scene(root, manifest, &error)) return test_failure(5, __LINE__);
    if (resolve_bundle_scene(root / "missing", manifest, &error)) return test_failure(6, __LINE__);
    const auto project = root / "Project";
    const auto export_base = root / "Exports";
    std::filesystem::create_directories(project / "Assets" / "Scenes");
    std::filesystem::create_directory(export_base);
    nengine::core::World world;
    const auto camera_entity = world.create("Camera");
    const auto body_entity = world.create("Sprite");
    auto* camera = world.add_component<nengine::render::Camera>(
        camera_entity, nengine::render::camera_type());
    auto* body = world.add_component<nengine::physics::Rigidbody2D>(
        body_entity, nengine::physics::rigidbody2d_type());
    const auto wav_guid = nengine::assets::AssetGuid::generate();
    const auto nanim_guid = nengine::assets::AssetGuid::generate();
    const auto controller_guid = nengine::assets::AssetGuid::generate();
    auto* sound = world.add_component<nengine::audio::AudioSource>(
        body_entity, nengine::audio::audio_source_type());
    if (!camera || !body || !sound) return test_failure(22, __LINE__);
    camera->vertical_fov_degrees = 71.f;
    body->mass = 2.5f;
    sound->volume = 0.6f;
    sound->clip = wav_guid;
    sound->loop = true;
    auto* animator = world.add_component<nengine::render::SpriteAnimator>(
        body_entity, nengine::render::sprite_animator_type());
    auto* sprite_renderer = world.add_component<nengine::render::SpriteRenderer>(
        body_entity, nengine::render::sprite_renderer_type());
    if (!animator || !sprite_renderer) return test_failure(34, __LINE__);
    animator->clip = nanim_guid;
    animator->controller = controller_guid;
    auto* behaviour = world.add_component<nengine::scripting::ScriptBehaviour>(
        body_entity, nengine::scripting::script_behaviour_type());
    if (!behaviour) return test_failure(25, __LINE__);
    behaviour->type_name = "Game.Player";
    nengine::core::ComponentSerializationRegistry codecs;
    if (!nengine::render::register_component_serializers(codecs) ||
        !nengine::physics::register_component_serializers(codecs) ||
        !nengine::audio::register_component_serializers(codecs) ||
        !nengine::scripting::register_component_serializers(codecs)) return test_failure(23, __LINE__);
    auto scene = nengine::core::SceneSerializer::capture(world, "Main", &codecs);
    if (!nengine::core::SceneSerializer::save_file(
        scene, project / "Assets" / "Scenes" / "Main.nscene")) return test_failure(7, __LINE__);
    std::filesystem::create_directories(project / "Assets" / "Textures");
    { std::ofstream file(project / "Assets" / "Textures" / "hero.png");
      file << "texture test bytes"; }
    const auto texture_guid = nengine::assets::AssetGuid::generate();
    { std::ofstream meta(project / "Assets" / "Textures" / "hero.png.meta");
      meta << "NENGINE_META 1\nGUID " << texture_guid.to_string()
           << "\nIMPORTER " << std::quoted(std::string("NEngine.Texture"))
           << "\nEND_META\n"; }
    const auto material_guid = nengine::assets::AssetGuid::generate();
    const auto model_guid = nengine::assets::AssetGuid::generate();
    std::filesystem::create_directories(project / "Assets" / "Materials");
    { std::ofstream file(project / "Assets" / "Materials" / "default.nmat");
      file << "NENGINE_MATERIAL 1\nBASE_COLOR_TEXTURE "
           << std::quoted(texture_guid.to_string()) << "\nEND_MATERIAL\n"; }
    { std::ofstream meta(project / "Assets" / "Materials" / "default.nmat.meta");
      meta << "NENGINE_META 1\nGUID " << material_guid.to_string()
           << "\nIMPORTER \"NEngine.Material\"\nEND_META\n"; }
    std::filesystem::create_directories(project / "Assets" / "Models");
    { std::ofstream file(project / "Assets" / "Models" / "triangle.obj");
      file << "mtllib triangle.mtl\nv 0 0 0\nv 1 0 0\nv 0 1 0\n"
              "usemtl Color\nf 1 2 3\n"; }
    { std::ofstream mtl(project / "Assets" / "Models" / "triangle.mtl");
      mtl << "newmtl Color\nKd 0.2 0.4 0.8\nd 1.0\n"; }
    { std::ofstream meta(project / "Assets" / "Models" / "triangle.obj.meta");
      meta << "NENGINE_META 1\nGUID " << model_guid.to_string()
           << "\nIMPORTER \"NEngine.Model\"\nEND_META\n"; }
    std::filesystem::create_directories(project / "Assets" / "Animations");
    { std::ofstream file(project / "Assets" / "Animations" / "walk.nanim");
      file << "NENGINE_SPRITE_ANIMATION 1\nFRAMES 2\n"
           << "FRAME " << std::quoted(texture_guid.to_string()) << " 0.1\n"
           << "FRAME " << std::quoted(texture_guid.to_string()) << " 0.2\n"
           << "END_SPRITE_ANIMATION\n"; }
    { std::ofstream meta(project / "Assets" / "Animations" / "walk.nanim.meta");
      meta << "NENGINE_META 1\nGUID " << nanim_guid.to_string()
           << "\nIMPORTER " << std::quoted(std::string("NEngine.SpriteAnimation"))
           << "\nEND_META\n"; }
    { std::ofstream file(project / "Assets" / "Animations" / "walk.nanimctrl");
      file << "NENGINE_ANIMATION_CONTROLLER 1\nDEFAULT \"Idle\"\n"
           << "STATES 2\nSTATE \"Idle\" " << std::quoted(nanim_guid.to_string())
           << " 1 1\nSTATE \"Run\" " << std::quoted(nanim_guid.to_string())
           << " 1 2\nTRANSITIONS 1\n"
           << "TRANSITION \"Idle\" \"Run\" \"moving\" 1 0\n"
           << "END_ANIMATION_CONTROLLER\n"; }
    { std::ofstream meta(project / "Assets" / "Animations" / "walk.nanimctrl.meta");
      meta << "NENGINE_META 1\nGUID " << controller_guid.to_string()
           << "\nIMPORTER " << std::quoted(std::string("NEngine.AnimationController"))
           << "\nEND_META\n"; }
    std::filesystem::create_directories(project / "Assets" / "Sounds");
    { const std::array<std::uint8_t, 46> wav{
        'R','I','F','F',38,0,0,0,'W','A','V','E','f','m','t',' ',
        16,0,0,0,1,0,1,0,64,31,0,0,128,62,0,0,
        2,0,16,0,'d','a','t','a',2,0,0,0,0,0};
      std::ofstream file(project / "Assets" / "Sounds" / "ping.wav", std::ios::binary);
      file.write(reinterpret_cast<const char*>(wav.data()), wav.size()); }
    { std::ofstream meta(project / "Assets" / "Sounds" / "ping.wav.meta");
      meta << "NENGINE_META 1\nGUID " << wav_guid.to_string()
           << "\nIMPORTER " << std::quoted(std::string("NEngine.Audio"))
           << "\nEND_META\n"; }
    // Additional bundled sources deliberately unreferenced by the saved
    // Scene exercise Player clip loading after a managed property change.
    const auto dynamic_wav_guid = nengine::assets::AssetGuid::generate();
    std::filesystem::copy_file(
        project / "Assets" / "Sounds" / "ping.wav",
        project / "Assets" / "Sounds" / "extra.wav");
    { std::ofstream meta(project / "Assets" / "Sounds" / "extra.wav.meta");
      meta << "NENGINE_META 1\nGUID " << dynamic_wav_guid.to_string()
           << "\nIMPORTER \"NEngine.Audio\"\nEND_META\n"; }
    const auto dynamic_sprite_guid = nengine::assets::AssetGuid::generate();
    { std::ofstream file(project / "Assets" / "Animations" / "extra.nanim");
      file << "NENGINE_SPRITE_ANIMATION 1\nFRAMES 1\n"
           << "FRAME " << std::quoted(texture_guid.to_string()) << " 0.1\n"
           << "END_SPRITE_ANIMATION\n"; }
    { std::ofstream meta(project / "Assets" / "Animations" / "extra.nanim.meta");
      meta << "NENGINE_META 1\nGUID " << dynamic_sprite_guid.to_string()
           << "\nIMPORTER \"NEngine.SpriteAnimation\"\nEND_META\n"; }
    { std::ofstream file(project / "Assets" / "Textures" / "ignore.cs");
      file << "do not ship source code"; }
    // A conventional Canvas source becomes part of the Player bundle.
    std::filesystem::create_directories(project / "Assets" / "UI");
    nengine::ui::UiCanvas canvas_fixture;
    const auto canvas_button = canvas_fixture.add(nengine::ui::WidgetKind::Button);
    if (canvas_button == nengine::ui::invalid_widget ||
        !canvas_fixture.set_name(canvas_button, "PlayButton") ||
        !canvas_fixture.set_text(canvas_button, "Play")) return test_failure(170, __LINE__);
    { std::ofstream out(project / "Assets" / "UI" / "Main.ncanvas");
      if (!nengine::ui::write_canvas(out, canvas_fixture, &error))
          return test_failure(171, __LINE__); }
    const auto canvas_guid = nengine::assets::AssetGuid::generate();
    { std::ofstream meta(project / "Assets" / "UI" / "Main.ncanvas.meta");
      meta << "NENGINE_META 1\nGUID " << canvas_guid.to_string()
           << "\nIMPORTER \"NEngine.Canvas\"\nEND_META\n"; }
    nengine::input::ActionMap exported_actions;
    if (!exported_actions.bind("Jump", nengine::input::Key::Space) ||
        !exported_actions.bind("Jump", nengine::input::Key::A) ||
        !exported_actions.bind("Fire", nengine::input::Key::MouseLeft) ||
        !nengine::input::save_action_map_file(
            project / "Input.ninput", exported_actions, &error))
        return test_failure(146, __LINE__);

    ExportSettings settings;
    settings.project_root = project;
    settings.startup_scene = "Assets/Scenes/Main.nscene";
    settings.destination = export_base / "Build";
    settings.title = "Test Player";
    ExportReport report;
    if (!export_player_data(settings, report, &error) ||
        report.scene_objects != 2u ||
        report.copied_assets < 2u ||
        report.cooked_textures < 1u ||
        report.cooked_materials != 1u ||
        report.cooked_models != 1u ||
        report.generated_materials != 1u ||
        report.generated_textures != 1u ||
        !std::filesystem::exists(
            report.bundle_root / "Content" / "Assets" / "Textures" / "hero.png") ||
        std::filesystem::exists(
            report.bundle_root / "Content" / "Assets" / "Textures" / "ignore.cs") ||
        !std::filesystem::exists(
            report.bundle_root / "Content" / "Main.nscene"))
        return test_failure(8, __LINE__);
    nengine::input::ActionMap bundled_actions;
    if (!nengine::input::load_action_map_file(
            report.bundle_root / "Content" / "Input.ninput", bundled_actions, &error) ||
        bundled_actions.action_count() != 2u ||
        !bundled_actions.contains("Jump") || !bundled_actions.contains("Fire"))
        return test_failure(147, __LINE__);
    BundleManifest built;
    if (!load_bundle(report.bundle_root / "NEngine.player", built, &error) ||
        built.title != settings.title ||
        !resolve_bundle_scene(report.bundle_root, built, &error))
        return test_failure(9, __LINE__);
    if (!std::filesystem::is_regular_file(
            report.bundle_root / "Content" / "Assets" / "UI" / "Main.ncanvas"))
        return test_failure(172, __LINE__);
    PlayerRuntime runtime;
    if (!runtime.load(report.bundle_root, &error) ||
        !runtime.ready() || runtime.entity_count() != 2u ||
        runtime.pending_script_count() != 1u ||
        !runtime.assets().source(texture_guid)) return test_failure(13, __LINE__);
    {
        PlayerRuntime dynamic;
        if (!dynamic.load(report.bundle_root, &error) ||
            dynamic.audio_clip(dynamic_wav_guid))
            return test_failure(118, __LINE__);
        const auto entity = dynamic.world().create("Runtime asset reassignment");
        auto* sound = dynamic.world().add_component<nengine::audio::AudioSource>(
            entity, nengine::audio::audio_source_type());
        auto* animator = dynamic.world().add_component<
            nengine::render::SpriteAnimator>(
            entity, nengine::render::sprite_animator_type());
        auto* renderer = dynamic.world().add_component<
            nengine::render::SpriteRenderer>(
            entity, nengine::render::sprite_renderer_type());
        if (!sound || !animator || !renderer) return test_failure(119, __LINE__);
        sound->clip = dynamic_wav_guid;
        sound->playing = true;
        animator->clip = dynamic_sprite_guid;
        animator->playing = true;
        if (const auto f = dynamic.advance(1.f / 60.f);
            !dynamic.audio_clip(dynamic_wav_guid) ||
            f.sprite_sampled < 1u || renderer->texture != texture_guid)
            return test_failure(120, __LINE__);
        // A live C# Controller setter must install a state graph and its
        // referenced clips; clearing it must stop graph-driven transitions.
        animator->controller = controller_guid;
        animator->bool_parameters["moving"] = true;
        dynamic.advance(1.f / 60.f); // establish default state
        const auto graph_step = dynamic.advance(1.f / 60.f);
        if (!graph_step.controller_transitions ||
            animator->speed != 2.0f || animator->clip != nanim_guid)
            return test_failure(126, __LINE__);
        animator->controller = {};
        animator->clip = dynamic_sprite_guid;
        animator->time_seconds = 0.0f;
        const auto cleared_graph = dynamic.advance(1.f / 60.f);
        if (cleared_graph.controller_transitions != 0u ||
            animator->clip != dynamic_sprite_guid)
            return test_failure(127, __LINE__);
        animator->playing = true;
        animator->loop = false;
        animator->speed = 1.0f;
        animator->time_seconds = 0.0f;
        dynamic.advance(0.2f);
        if (animator->playing ||
            std::abs(animator->time_seconds - 0.1f) > 0.0001f)
            return test_failure(128, __LINE__);
        animator->playing = true;
        animator->loop = true;
        animator->speed = 1.0e9f;
        animator->time_seconds = 3.0e38f;
        const auto extreme = dynamic.advance(0.25f);
        if (!std::isfinite(animator->time_seconds) ||
            animator->time_seconds < 0.0f ||
            animator->time_seconds >= 0.1f ||
            extreme.sprite_sampled == 0u)
            return test_failure(129, __LINE__);
        renderer->enabled = false;
        renderer->texture = {};
        dynamic.advance(1.f / 60.f);
        // The startup scene contains other, still-enabled sprite renderers.
        // Only the disabled renderer's output is required to stay untouched.
        if (renderer->texture.valid())
            return test_failure(130, __LINE__);
        renderer->enabled = true;
        const auto missing_guid = nengine::assets::AssetGuid::generate();
        sound->clip = missing_guid;
        animator->clip = missing_guid;
        dynamic.advance(1.f / 60.f);
        dynamic.advance(1.f / 60.f);
        if (dynamic.audio_clip(missing_guid) != nullptr)
            return test_failure(121, __LINE__);
        if (!dynamic.load(report.bundle_root, &error) ||
            dynamic.entity_count() != 2u ||
            dynamic.audio_clip(dynamic_wav_guid) != nullptr)
            return test_failure(131, __LINE__);
        if (!dynamic.advance(1.f / 60.f).controller_transitions)
            return test_failure(132, __LINE__);
        dynamic.unload();
    }
    if (!runtime.actions().contains("Jump") ||
        !runtime.actions().contains("Fire") ||
        runtime.actions().action_count() != 2u)
        return test_failure(148, __LINE__);
    runtime.begin_input_frame();
    runtime.input().set_key(nengine::input::Key::A, true);
    if (!runtime.input().held(nengine::input::Key::A) ||
        !runtime.input().pressed(nengine::input::Key::A) ||
        !runtime.actions().held("Jump", runtime.input()) ||
        !runtime.actions().pressed("Jump", runtime.input()) ||
        runtime.actions().held("Fire", runtime.input()))
        return test_failure(47, __LINE__);
    runtime.begin_input_frame();
    runtime.input().set_key(nengine::input::Key::A, true);
    if (runtime.input().pressed(nengine::input::Key::A) ||
        runtime.actions().pressed("Jump", runtime.input()) ||
        !runtime.actions().held("Jump", runtime.input()))
        return test_failure(48, __LINE__);
    runtime.begin_input_frame();
    runtime.input().set_key(nengine::input::Key::A, false);
    if (!runtime.input().released(nengine::input::Key::A) ||
        !runtime.actions().released("Jump", runtime.input()) ||
        runtime.actions().held("Jump", runtime.input()) ||
        runtime.actions().pressed("NoSuchAction", runtime.input()))
        return test_failure(49, __LINE__);
    // Invalid action map reload must never replace a running Scene or its actions.
    const auto staged_actions = report.bundle_root / "Content" / "Input.ninput";
    {
        std::ofstream malformed(staged_actions, std::ios::trunc);
        malformed << "NENGINE_ACTION_MAP 9\n";
    }
    if (runtime.load(report.bundle_root, &error) ||
        !runtime.ready() || runtime.entity_count() != 2u ||
        !runtime.actions().contains("Jump"))
        return test_failure(149, __LINE__);
    std::string recovered_actions;
    if (!exported_actions.serialize(recovered_actions, &error))
        return test_failure(150, __LINE__);
    {
        std::ofstream restored_map(staged_actions, std::ios::trunc);
        restored_map << recovered_actions;
    }
    if (!runtime.load(report.bundle_root, &error) ||
        !runtime.ready() || !runtime.actions().contains("Jump"))
        return test_failure(151, __LINE__);
    // Older player bundles without an Input.ninput remain supported.
    std::filesystem::remove(staged_actions);
    if (!runtime.load(report.bundle_root, &error) ||
        runtime.actions().action_count() != 0u)
        return test_failure(152, __LINE__);
    {
        std::ofstream restored_map(staged_actions, std::ios::trunc);
        restored_map << recovered_actions;
    }
    if (!runtime.load(report.bundle_root, &error) ||
        !runtime.actions().contains("Jump"))
        return test_failure(153, __LINE__);
    if (runtime.managed_ready() ||
        runtime.initialize_managed(&error) ||
        !runtime.ready() || runtime.entity_count() != 2u ||
        runtime.managed_ready())
        return test_failure(44, __LINE__);
    bool restored_camera = false;
    bool restored_body = false;
    for (const auto entity : runtime.world().entities()) {
        if (const auto* camera = runtime.world().get_component<
                nengine::render::Camera>(
                entity, nengine::render::camera_type()))
            restored_camera = camera->vertical_fov_degrees == 71.f;
        if (const auto* body = runtime.world().get_component<
                nengine::physics::Rigidbody2D>(
                entity, nengine::physics::rigidbody2d_type()))
            restored_body = body->mass == 2.5f &&
                runtime.world().get_component<nengine::audio::AudioSource>(
                    entity, nengine::audio::audio_source_type()) != nullptr;
    }
    if (!restored_camera || !restored_body) return test_failure(24, __LINE__);
    const auto first_entity = runtime.world().entities().front();
    nengine::core::PropertyValue property;
    if (!read_player_transform_property(
            runtime.world(), first_entity, "NEngine.Transform",
            "Local Position", property) ||
        !std::holds_alternative<nengine::core::Vec3>(property))
        return test_failure(50, __LINE__);
    if (!write_player_transform_property(
            runtime.world(), first_entity, "NEngine.Transform",
            "Local Position", nengine::core::Vec3{3, 4, 5}) ||
        !read_player_transform_property(
            runtime.world(), first_entity, "NEngine.Transform",
            "Local Position", property) ||
        std::get<nengine::core::Vec3>(property).x != 3.f)
        return test_failure(51, __LINE__);
    if (write_player_transform_property(
            runtime.world(), first_entity, "NEngine.Transform",
            "Local Position", nengine::core::Vec3{
                std::numeric_limits<float>::infinity(), 0, 0}) ||
        write_player_transform_property(
            runtime.world(), first_entity, "NEngine.Transform",
            "Unknown", nengine::core::Vec3{0, 0, 0}) ||
        write_player_transform_property(
            runtime.world(), first_entity, "NEngine.Unknown",
            "Local Scale", nengine::core::Vec3{1, 1, 1}))
        return test_failure(52, __LINE__);
    bool body_property_ok = false;
    for (const auto entity : runtime.world().entities()) {
        if (!runtime.world().get_component<nengine::physics::Rigidbody2D>(
                entity, nengine::physics::rigidbody2d_type())) continue;
        nengine::core::PropertyValue mass;
        body_property_ok = read_player_transform_property(
            runtime.world(), entity, "NEngine.Rigidbody2D", "Mass", mass) &&
            write_player_transform_property(
                runtime.world(), entity, "NEngine.Rigidbody2D",
                "Mass", 3.5) &&
            !write_player_transform_property(
                runtime.world(), entity, "NEngine.Rigidbody2D",
                "Mass", -1.0) &&
            std::get<double>(mass) == 2.5;
    }
    if (!body_property_ok) return test_failure(53, __LINE__);
    bool audio_property_ok = false;
    for (const auto entity : runtime.world().entities()) {
        if (!runtime.world().get_component<nengine::audio::AudioSource>(
                entity, nengine::audio::audio_source_type())) continue;
        nengine::core::PropertyValue volume;
        audio_property_ok = read_player_transform_property(
            runtime.world(), entity, "NEngine.AudioSource",
            "Volume", volume) &&
            write_player_transform_property(
                runtime.world(), entity, "NEngine.AudioSource",
                "Volume", 0.25) &&
            !write_player_transform_property(
                runtime.world(), entity, "NEngine.AudioSource",
                "Volume", 1.5) &&
            std::get<double>(volume) > 0.5;
    }
    if (!audio_property_ok) return test_failure(54, __LINE__);
    // Exported Player has to expose the same native 3D/2D Rigidbody
    // properties used by generated C# gameplay, including wake semantics.
    const auto bridge_entity = runtime.world().create("Player Rigidbody bridge");
    if (!runtime.world().add_component<nengine::physics::Rigidbody>(
            bridge_entity, nengine::physics::rigidbody_type()) ||
        !runtime.world().add_component<nengine::physics::Rigidbody2D>(
            bridge_entity, nengine::physics::rigidbody2d_type()))
        return test_failure(101, __LINE__);
    for (const auto* kind : {"NEngine.Rigidbody", "NEngine.Rigidbody2D"}) {
        nengine::core::PropertyValue value;
        const auto read = [&](std::string_view name) {
            return read_player_transform_property(
                runtime.world(), bridge_entity, kind, name, value);
        };
        const auto write = [&](std::string_view name,
                               const nengine::core::PropertyValue& input) {
            return write_player_transform_property(
                runtime.world(), bridge_entity, kind, name, input);
        };
        if (!write("Use Gravity", false) || !read("Use Gravity") ||
            std::get<bool>(value) ||
            !write("Is Kinematic", true) || !read("Is Kinematic") ||
            !std::get<bool>(value) ||
            !write("Mass", 7.25) || !read("Mass") ||
            std::get<double>(value) != 7.25 ||
            !write("Gravity Scale", 0.0) || !read("Gravity Scale") ||
            std::get<double>(value) != 0.0 ||
            !write("Sleep Threshold", 0.125) ||
            !read("Sleep Threshold") ||
            std::get<double>(value) != 0.125)
            return test_failure(102, __LINE__);
        if (!write("Linear Velocity", nengine::core::Vec3{1, 2, 3}) ||
            !write("Angular Velocity", nengine::core::Vec3{4, 5, 6}) ||
            !read("Angular Velocity") ||
            std::get<nengine::core::Vec3>(value).z != 6.f ||
            !write("Sleeping", true) || !read("Sleeping") ||
            !std::get<bool>(value) ||
            !read("Linear Velocity") ||
            std::get<nengine::core::Vec3>(value).x != 0.f)
            return test_failure(103, __LINE__);
        if (!write("Linear Velocity", nengine::core::Vec3{2, 0, 0}) ||
            !read("Sleeping") || std::get<bool>(value) ||
            !write("Sleeping", true) ||
            !write("Allow Sleep", false) || !read("Sleeping") ||
            std::get<bool>(value) ||
            !write("Sleeping", true) || !read("Sleeping") ||
            std::get<bool>(value))
            return test_failure(104, __LINE__);
        if (write("Mass", -1.0) || write("Mass", 0.0) ||
            write("Mass", 1.0e-300) ||
            write("Gravity Scale", -0.5) ||
            write("Mass", std::numeric_limits<double>::infinity()) ||
            write("Sleep Threshold", std::numeric_limits<double>::quiet_NaN()) ||
            write("Linear Velocity", nengine::core::Vec3{
                std::numeric_limits<float>::infinity(), 0, 0}) ||
            write("Angular Velocity", nengine::core::Vec3{
                0, std::numeric_limits<float>::quiet_NaN(), 0}) ||
            write("Enabled", 1.0) ||
            write("Made Up", true) || read("Made Up"))
            return test_failure(105, __LINE__);
    }
    if (read_player_transform_property(
            runtime.world(), first_entity, "NEngine.Rigidbody",
            "Mass", property) ||
        write_player_transform_property(
            runtime.world(), first_entity, "NEngine.Rigidbody",
            "Mass", 2.0))
        return test_failure(106, __LINE__);
    // Exported managed Camera/Light/SpriteRenderer/MeshRenderer must not
    // silently fall back to generated C# defaults when the Player is running.
    const auto render_entity = runtime.world().create("Player render bridge");
    if (!runtime.world().add_component<nengine::render::Camera>(
            render_entity, nengine::render::camera_type()) ||
        !runtime.world().add_component<nengine::render::Light>(
            render_entity, nengine::render::light_type()) ||
        !runtime.world().add_component<nengine::render::SpriteRenderer>(
            render_entity, nengine::render::sprite_renderer_type()) ||
        !runtime.world().add_component<nengine::render::MeshRenderer>(
            render_entity, nengine::render::mesh_renderer_type()))
        return test_failure(107, __LINE__);
    {
        nengine::core::PropertyValue prop;
        const auto read = [&](std::string_view comp, std::string_view name) {
            return read_player_transform_property(
                runtime.world(), render_entity, comp, name, prop);
        };
        const auto write = [&](std::string_view comp, std::string_view name,
                               const nengine::core::PropertyValue& input) {
            return write_player_transform_property(
                runtime.world(), render_entity, comp, name, input);
        };
        if (!write("NEngine.Camera", "Projection", std::int64_t{1}) ||
            !read("NEngine.Camera", "Projection") ||
            std::get<std::int64_t>(prop) != 1 ||
            !write("NEngine.Camera", "Vertical FOV", 80.0) ||
            !write("NEngine.Camera", "Near Clip", 0.5) ||
            !write("NEngine.Camera", "Far Clip", 250.0) ||
            !read("NEngine.Camera", "Far Clip") ||
            std::get<double>(prop) != 250.0 ||
            write("NEngine.Camera", "Projection", std::int64_t{2}) ||
            write("NEngine.Camera", "Far Clip", 0.1) ||
            write("NEngine.Camera", "Near Clip", 500.0) ||
            write("NEngine.Camera", "Vertical FOV",
                std::numeric_limits<double>::infinity()))
            return test_failure(108, __LINE__);
        if (!write("NEngine.Light", "Type", std::int64_t{2}) ||
            !write("NEngine.Light", "Color", nengine::core::Vec3{2.f, 1.f, 0.5f}) ||
            !write("NEngine.Light", "Intensity", 4.0) ||
            !read("NEngine.Light", "Color") ||
            std::get<nengine::core::Vec3>(prop).x != 2.f ||
            !write("NEngine.Light", "Cast Shadows", false) ||
            write("NEngine.Light", "Type", std::int64_t{8}) ||
            write("NEngine.Light", "Color", nengine::core::Vec3{-1.f, 0.f, 0.f}) ||
            write("NEngine.Light", "Intensity",
                std::numeric_limits<double>::quiet_NaN()))
            return test_failure(109, __LINE__);
        if (!write("NEngine.SpriteRenderer", "Texture",
                texture_guid.to_string()) ||
            !read("NEngine.SpriteRenderer", "Texture") ||
            std::get<std::string>(prop) != texture_guid.to_string() ||
            !write("NEngine.SpriteRenderer", "Pixels Per Unit", 75.0) ||
            !write("NEngine.SpriteRenderer", "Sort Order", std::int64_t{-42}) ||
            !write("NEngine.SpriteRenderer", "Flip X", true) ||
            !read("NEngine.SpriteRenderer", "Flip X") ||
            !std::get<bool>(prop) ||
            write("NEngine.SpriteRenderer", "Texture", std::string{"bad-guid"}) ||
            write("NEngine.SpriteRenderer", "Pixels Per Unit", 0.0) ||
            write("NEngine.SpriteRenderer", "Sort Order", 7.0))
            return test_failure(110, __LINE__);
        if (!write("NEngine.MeshRenderer", "Material",
                material_guid.to_string()) ||
            !read("NEngine.MeshRenderer", "Material") ||
            std::get<std::string>(prop) != material_guid.to_string() ||
            !write("NEngine.MeshRenderer", "Cast Shadows", false) ||
            !write("NEngine.MeshRenderer", "Mesh", std::string{}) ||
            !read("NEngine.MeshRenderer", "Mesh") ||
            !std::get<std::string>(prop).empty() ||
            write("NEngine.MeshRenderer", "Material", std::string{"../invalid"}) ||
            write("NEngine.MeshRenderer", "Enabled", 1.0) ||
            read("NEngine.MeshRenderer", "Unknown") ||
            write("NEngine.MeshRenderer", "Unknown", true))
            return test_failure(111, __LINE__);
        if (read_player_transform_property(
                runtime.world(), first_entity, "NEngine.Light",
                "Intensity", prop))
            return test_failure(112, __LINE__);
    }
    // Managed sprite animation is a separate component from SpriteRenderer.
    auto* managed_animator = runtime.world().add_component<
        nengine::render::SpriteAnimator>(
            render_entity, nengine::render::sprite_animator_type());
    if (!managed_animator) return test_failure(122, __LINE__);
    {
        nengine::core::PropertyValue property;
        const auto r = [&](std::string_view key) {
            return read_player_transform_property(runtime.world(), render_entity,
                "NEngine.SpriteAnimator", key, property);
        };
        const auto w = [&](std::string_view key,
                           const nengine::core::PropertyValue& value) {
            return write_player_transform_property(runtime.world(), render_entity,
                "NEngine.SpriteAnimator", key, value);
        };
        if (!w("Enabled", false) || !r("Enabled") || std::get<bool>(property) ||
            !w("Playing", false) || !r("Playing") || std::get<bool>(property) ||
            !w("Loop", false) || !r("Loop") || std::get<bool>(property) ||
            !w("Clip", nanim_guid.to_string()) ||
            !r("Clip") || std::get<std::string>(property) != nanim_guid.to_string() ||
            !w("Controller", controller_guid.to_string()) ||
            !r("Controller") || std::get<std::string>(property) != controller_guid.to_string() ||
            !w("Speed", 2.0) || !r("Speed") || std::get<double>(property) != 2.0 ||
            !w("Time", 3.5) || !r("Time") || std::get<double>(property) != 3.5 ||
            !w("Clip", nanim_guid.to_string()) || !r("Time") ||
            std::get<double>(property) != 3.5)
            return test_failure(123, __LINE__);
        if (!w("Bool Parameter", std::string{"moving=1"}) ||
            !r("Bool Parameter") ||
            std::get<std::string>(property) != "moving=1" ||
            !managed_animator->bool_parameters.at("moving") ||
            !w("Bool Parameter", std::string{"moving=0"}) ||
            managed_animator->bool_parameters.at("moving") ||
            w("Bool Parameter", std::string{"bad space=1"}) ||
            w("Bool Parameter", std::string{"foo=yes"}) ||
            w("Bool Parameter", std::string{"=1"}) ||
            w("Bool Parameter", std::string{"foo=2"}) ||
            w("Bool Parameter", std::string(129, 'a') + "=1"))
            return test_failure(125, __LINE__);
        if (w("Speed", -1.0) || w("Time", std::numeric_limits<double>::infinity()) ||
            w("Speed", std::numeric_limits<double>::quiet_NaN()) ||
            w("Controller", std::string{"../bad"}) ||
            w("Clip", 1.0) || w("Enabled", 1.0) || r("Unknown"))
            return test_failure(124, __LINE__);
    }
    // Native C# property bridge for exported 3D/2D physics colliders.
    // Keep the fixtures out of the subsequent exact-entity-count tests.
    const auto collider_entity = runtime.world().create("Managed Collider bridge");
    if (!runtime.world().add_component<nengine::physics::BoxCollider>(
            collider_entity, nengine::physics::box_collider_type()) ||
        !runtime.world().add_component<nengine::physics::SphereCollider>(
            collider_entity, nengine::physics::sphere_collider_type()) ||
        !runtime.world().add_component<nengine::physics::CapsuleCollider>(
            collider_entity, nengine::physics::capsule_collider_type()) ||
        !runtime.world().add_component<nengine::physics::BoxCollider2D>(
            collider_entity, nengine::physics::box_collider2d_type()) ||
        !runtime.world().add_component<nengine::physics::CircleCollider2D>(
            collider_entity, nengine::physics::circle_collider2d_type()) ||
        !runtime.world().add_component<nengine::physics::CapsuleCollider2D>(
            collider_entity, nengine::physics::capsule_collider2d_type()))
        return test_failure(133, __LINE__);
    {
        nengine::core::PropertyValue collider_value;
        const auto read = [&](std::string_view component, std::string_view property) {
            return read_player_transform_property(
                runtime.world(), collider_entity, component, property,
                collider_value);
        };
        const auto write = [&](std::string_view component, std::string_view property,
                               const nengine::core::PropertyValue& value) {
            return write_player_transform_property(
                runtime.world(), collider_entity, component, property, value);
        };
// 3D box geometry and collision filtering must roundtrip to native.
        if (!write("NEngine.BoxCollider", "Enabled", false) ||
            !write("NEngine.BoxCollider", "Is Trigger", true) ||
            !write("NEngine.BoxCollider", "Layer", std::int64_t{31}) ||
            !write("NEngine.BoxCollider", "Collision Mask", std::int64_t{4294967295LL}) ||
            !write("NEngine.BoxCollider", "Friction", 0.25) ||
            !write("NEngine.BoxCollider", "Restitution", 0.75) ||
            !write("NEngine.BoxCollider", "Center", nengine::core::Vec3{1, 2, 3}) ||
            !write("NEngine.BoxCollider", "Size", nengine::core::Vec3{4, 5, 6}) ||
            !read("NEngine.BoxCollider", "Size") ||
            std::get<nengine::core::Vec3>(collider_value).z != 6.f ||
            !read("NEngine.BoxCollider", "Layer") ||
            std::get<std::int64_t>(collider_value) != 31 ||
            !read("NEngine.BoxCollider", "Collision Mask") ||
            std::get<std::int64_t>(collider_value) != 4294967295LL ||
            !read("NEngine.BoxCollider", "Is Trigger") ||
            !std::get<bool>(collider_value))
            return test_failure(135, __LINE__);
        if (write("NEngine.BoxCollider", "Size", nengine::core::Vec3{0, 5, 6}) ||
            write("NEngine.BoxCollider", "Layer", std::int64_t{32}) ||
            write("NEngine.BoxCollider", "Collision Mask", std::int64_t{-1}) ||
            write("NEngine.BoxCollider", "Friction", 1.25) ||
            !read("NEngine.BoxCollider", "Size") ||
            std::get<nengine::core::Vec3>(collider_value).x != 4.f)
            return test_failure(136, __LINE__);
// 3D sphere's center, contact flags and radius cannot accept NaN/zero.
        if (!write("NEngine.SphereCollider", "Radius", 3.0) ||
            !write("NEngine.SphereCollider", "Center", nengine::core::Vec3{2, -4, 1}) ||
            !write("NEngine.SphereCollider", "Friction", 1.0) ||
            !read("NEngine.SphereCollider", "Radius") ||
            std::get<double>(collider_value) != 3.0 ||
            write("NEngine.SphereCollider", "Radius", 0.0) ||
            write("NEngine.SphereCollider", "Radius", -5.0) ||
            write("NEngine.SphereCollider", "Radius",
                  std::numeric_limits<double>::quiet_NaN()) ||
            write("NEngine.SphereCollider", "Center", nengine::core::Vec3{
                  std::numeric_limits<float>::infinity(), 0, 0}) ||
            !read("NEngine.SphereCollider", "Radius") ||
            std::get<double>(collider_value) != 3.0)
            return test_failure(137, __LINE__);
// 3D capsule cannot be set shorter than twice its radius.
        if (!write("NEngine.CapsuleCollider", "Height", 8.0) ||
            !write("NEngine.CapsuleCollider", "Radius", 2.0) ||
            !write("NEngine.CapsuleCollider", "Direction", std::int64_t{2}) ||
            !read("NEngine.CapsuleCollider", "Direction") ||
            std::get<std::int64_t>(collider_value) != 2 ||
            write("NEngine.CapsuleCollider", "Radius", 5.0) ||
            write("NEngine.CapsuleCollider", "Height", 3.0) ||
            write("NEngine.CapsuleCollider", "Direction", std::int64_t{3}) ||
            write("NEngine.CapsuleCollider", "Radius",
                  std::numeric_limits<double>::infinity()) ||
            !read("NEngine.CapsuleCollider", "Radius") ||
            std::get<double>(collider_value) != 2.0 ||
            !read("NEngine.CapsuleCollider", "Height") ||
            std::get<double>(collider_value) != 8.0)
            return test_failure(138, __LINE__);
// 2D native center and box dimensions must discard the unused Z axis.
        if (!write("NEngine.BoxCollider2D", "Center",
                  nengine::core::Vec3{4, 6, 99}) ||
            !write("NEngine.BoxCollider2D", "Size",
                  nengine::core::Vec3{2, 3, 17}) ||
            !read("NEngine.BoxCollider2D", "Center") ||
            std::get<nengine::core::Vec3>(collider_value).z != 0.f ||
            !read("NEngine.BoxCollider2D", "Size") ||
            std::get<nengine::core::Vec3>(collider_value).z != 0.f ||
            write("NEngine.BoxCollider2D", "Size",
                  nengine::core::Vec3{2, -1, 0}) ||
            write("NEngine.BoxCollider2D", "Center",
                  nengine::core::Vec3{1.0e9f, 0, 0}) ||
            !read("NEngine.BoxCollider2D", "Size") ||
            std::get<nengine::core::Vec3>(collider_value).y != 3.f)
            return test_failure(139, __LINE__);
// Circular 2D primitive supports contact coefficients and radius.
        if (!write("NEngine.CircleCollider2D", "Radius", 0.75) ||
            !write("NEngine.CircleCollider2D", "Is Trigger", true) ||
            !write("NEngine.CircleCollider2D", "Restitution", 0.5) ||
            !read("NEngine.CircleCollider2D", "Radius") ||
            std::get<double>(collider_value) != 0.75 ||
            write("NEngine.CircleCollider2D", "Radius", 0.0) ||
            write("NEngine.CircleCollider2D", "Radius", 1.0e20) ||
            write("NEngine.CircleCollider2D", "Restitution", -0.1) ||
            !read("NEngine.CircleCollider2D", "Radius") ||
            std::get<double>(collider_value) != 0.75)
            return test_failure(140, __LINE__);
// 2D capsule orientation must be horizontal/vertical and Z remains 0.
        if (!write("NEngine.CapsuleCollider2D", "Size",
                  nengine::core::Vec3{1.5f, 4.f, 22.f}) ||
            !write("NEngine.CapsuleCollider2D", "Direction", std::int64_t{1}) ||
            !read("NEngine.CapsuleCollider2D", "Size") ||
            std::get<nengine::core::Vec3>(collider_value).z != 0.f ||
            !read("NEngine.CapsuleCollider2D", "Direction") ||
            std::get<std::int64_t>(collider_value) != 1 ||
            write("NEngine.CapsuleCollider2D", "Direction", std::int64_t{2}) ||
            write("NEngine.CapsuleCollider2D", "Size",
                  nengine::core::Vec3{1.5f, 0.f, 0.f}) ||
            !read("NEngine.CapsuleCollider2D", "Direction") ||
            std::get<std::int64_t>(collider_value) != 1)
            return test_failure(141, __LINE__);
// Unknown properties and absent components must fail closed,
        // while invalid writes cannot replace any accepted native state.
        const auto absent_collider = runtime.world().create("No Collider");
        const auto missing_read = read_player_transform_property(
            runtime.world(), absent_collider, "NEngine.CapsuleCollider2D",
            "Direction", collider_value);
        const auto missing_write = write_player_transform_property(
            runtime.world(), absent_collider, "NEngine.BoxCollider", "Enabled", false);
        if (missing_read || missing_write ||
            !runtime.world().destroy(absent_collider))
            return test_failure(142, __LINE__);
        for (const auto* type : {
            "NEngine.BoxCollider", "NEngine.SphereCollider",
            "NEngine.CapsuleCollider", "NEngine.BoxCollider2D",
            "NEngine.CircleCollider2D", "NEngine.CapsuleCollider2D"}) {
            if (!write(type, "Enabled", true) ||
                !write(type, "Layer", std::int64_t{3}) ||
                !read(type, "Layer") ||
                std::get<std::int64_t>(collider_value) != 3 ||
                write(type, "Layer", 3.0) ||
                write(type, "Collision Mask", std::int64_t{4294967296LL}) ||
                write(type, "Enabled", std::int64_t{1}) ||
                write(type, "Friction", std::numeric_limits<double>::infinity()) ||
                write(type, "Center", nengine::core::Vec3{
                    0, std::numeric_limits<float>::quiet_NaN(), 0}) ||
                write(type, "Imaginary Field", true) ||
                read(type, "Imaginary Field") ||
                !read(type, "Layer") ||
                std::get<std::int64_t>(collider_value) != 3)
                return test_failure(143, __LINE__);
        }
        // COLLIDER_CASES
    }
    if (!runtime.world().destroy(collider_entity))
        return test_failure(134, __LINE__);
    // The managed Player collider bridge must also affect actual physics
    // queries, not merely return changed values through a property getter.
    const auto ray_entity = runtime.world().create("Managed 2D ray target");
    if (!runtime.world().add_component<nengine::physics::BoxCollider2D>(
            ray_entity, nengine::physics::box_collider2d_type()) ||
        !write_player_transform_property(runtime.world(), ray_entity,
            "NEngine.BoxCollider2D", "Layer", std::int64_t{3}) ||
        !write_player_transform_property(runtime.world(), ray_entity,
            "NEngine.BoxCollider2D", "Is Trigger", true))
        return test_failure(144, __LINE__);
    const auto ray = [&](bool triggers, std::uint32_t layers) {
        return nengine::physics::raycast_2d(
            runtime.world(), {-5.f, 0.f}, {1.f, 0.f},
            10.f, triggers, layers);
    };
    if (ray(false, 1u << 3) || ray(true, 1u << 2) ||
        !ray(true, 1u << 3) ||
        ray(true, 1u << 3)->entity != ray_entity ||
        !write_player_transform_property(runtime.world(), ray_entity,
            "NEngine.BoxCollider2D", "Enabled", false) ||
        ray(true, 1u << 3) ||
        !runtime.world().destroy(ray_entity))
        return test_failure(145, __LINE__);
    // Audio C# property parity in a packaged Player: clip changes must
    // reset playback and bad inputs must not mutate accepted state.
    const auto audio_entity = runtime.world().create("Managed audio bridge");
    if (!runtime.world().add_component<nengine::audio::AudioSource>(
            audio_entity, nengine::audio::audio_source_type()) ||
        !runtime.world().add_component<nengine::audio::AudioListener>(
            audio_entity, nengine::audio::audio_listener_type()))
        return test_failure(114, __LINE__);
    {
        nengine::core::PropertyValue prop;
        const auto read = [&](std::string_view comp, std::string_view name) {
            return read_player_transform_property(
                runtime.world(), audio_entity, comp, name, prop);
        };
        const auto write = [&](std::string_view comp, std::string_view name,
                               const nengine::core::PropertyValue& value) {
            return write_player_transform_property(
                runtime.world(), audio_entity, comp, name, value);
        };
        if (!write("NEngine.AudioSource", "Clip", wav_guid.to_string()) ||
            !read("NEngine.AudioSource", "Clip") ||
            std::get<std::string>(prop) != wav_guid.to_string() ||
            !write("NEngine.AudioSource", "Play On Awake", false) ||
            !write("NEngine.AudioSource", "Spatialize", true) ||
            !write("NEngine.AudioSource", "Volume", 0.75) ||
            !write("NEngine.AudioSource", "Pitch", 2.0) ||
            !write("NEngine.AudioSource", "Pan Stereo", -0.25) ||
            !write("NEngine.AudioSource", "Min Distance", 3.0) ||
            !write("NEngine.AudioSource", "Max Distance", 20.0) ||
            !write("NEngine.AudioSource", "Time", 4.0) ||
            !write("NEngine.AudioSource", "Playing", true) ||
            !read("NEngine.AudioSource", "Time") ||
            std::get<double>(prop) != 4.0)
            return test_failure(115, __LINE__);
        if (write("NEngine.AudioSource", "Clip", std::string{"bad"}) ||
            write("NEngine.AudioSource", "Volume", 2.0) ||
            write("NEngine.AudioSource", "Pitch", -1.0) ||
            write("NEngine.AudioSource", "Pan Stereo",
                std::numeric_limits<double>::infinity()) ||
            write("NEngine.AudioSource", "Min Distance", 25.0) ||
            write("NEngine.AudioSource", "Max Distance", 1.0) ||
            write("NEngine.AudioSource", "Time",
                std::numeric_limits<double>::quiet_NaN()) ||
            !read("NEngine.AudioSource", "Clip") ||
            std::get<std::string>(prop) != wav_guid.to_string())
            return test_failure(116, __LINE__);
        const auto replacement = nengine::assets::AssetGuid::generate();
        if (!write("NEngine.AudioSource", "Clip", replacement.to_string()) ||
            !read("NEngine.AudioSource", "Playing") || std::get<bool>(prop) ||
            !read("NEngine.AudioSource", "Time") ||
            std::get<double>(prop) != 0.0 ||
            !write("NEngine.AudioListener", "Enabled", false) ||
            !write("NEngine.AudioListener", "Volume", 0.25) ||
            !read("NEngine.AudioListener", "Volume") ||
            std::get<double>(prop) != 0.25 ||
            write("NEngine.AudioListener", "Volume", -0.1) ||
            write("NEngine.AudioListener", "Enabled", 1.0) ||
            read("NEngine.AudioListener", "Playing") ||
            !runtime.world().destroy(audio_entity))
            return test_failure(117, __LINE__);
    }
    // Keep bridge fixtures out of the later fixed-step integration tests:
    // those assert exact entity counts and one simulated Rigidbody2D.
    if (!runtime.world().destroy(bridge_entity) ||
        !runtime.world().destroy(render_entity))
        return test_failure(113, __LINE__);
    const auto ui_button = runtime.ui().find("PlayButton");
    if (!runtime.ui().ready() || ui_button != canvas_button ||
        runtime.ui().query(ui_button, 0) != 1 ||
        runtime.ui().query(ui_button, 1) != 1 ||
        runtime.ui().get_text(ui_button) != std::optional<std::string>{"Play"} ||
        runtime.ui().find("Play") != canvas_button)
        return test_failure(173, __LINE__);
    const auto decoded_ping = runtime.assets().load_wav(wav_guid);
    const auto derived_material =
        nengine::assets::derive_subasset_guid(model_guid, "obj-material", 0u);
    const auto derived_texture =
        nengine::assets::derive_subasset_guid(model_guid, "obj-base-color", 0u);
    const auto derived_material_assets =
        runtime.assets().cooked_artifacts(derived_material);
    const auto derived_texture_assets =
        runtime.assets().cooked_artifacts(derived_texture);
    if (runtime.assets().generated_subasset_count() != 2u ||
        !derived_material_assets ||
        derived_material_assets->importer_id != "NEngine.Material" ||
        derived_material_assets->artifacts.size() != 1u ||
        !derived_texture_assets ||
        derived_texture_assets->importer_id != "NEngine.Texture" ||
        derived_texture_assets->artifacts.size() != 2u ||
        derived_texture_assets->artifacts[1].role != "texture-descriptor" ||
        !runtime.assets().source(derived_material) ||
        !runtime.assets().source(derived_texture))
        return test_failure(162, __LINE__);
    const auto resolved_generated_material =
        nengine::render::resolve_material_asset(
            derived_material, *derived_material_assets, &error);
    const auto resolved_generated_texture =
        nengine::render::resolve_texture_asset(
            derived_texture, *derived_texture_assets, &error);
    nengine::render::DecodedTextureData generated_pixels;
    if (!resolved_generated_material ||
        resolved_generated_material->material.base_color_texture != derived_texture ||
        !resolved_generated_texture ||
        !nengine::render::decode_texture_rgba8(
            *resolved_generated_texture, generated_pixels, &error) ||
        generated_pixels.width != 1u || generated_pixels.height != 1u ||
        !generated_pixels.valid())
        return test_failure(164, __LINE__);
    // Corrupting one exported index must fail a new catalog bootstrap
    // without silently dropping the offending material.
    const auto real_index = report.bundle_root / "Content" / "Cooked" /
        model_guid.to_string() / "subassets.nasset";
    std::string saved_index;
    { std::ifstream in(real_index, std::ios::binary);
      saved_index.assign(std::istreambuf_iterator<char>(in),
                         std::istreambuf_iterator<char>()); }
    if (saved_index.empty()) return test_failure(165, __LINE__);
    { std::ofstream out(real_index, std::ios::binary | std::ios::trunc);
      out << "NENGINE_PLAYER_SUBASSETS 99\n"; }
    PlayerAssets tampered_catalog;
    if (tampered_catalog.open(report.bundle_root, &error) ||
        tampered_catalog.generated_subasset_count() != 0u)
        return test_failure(166, __LINE__);
    { std::ofstream out(real_index, std::ios::binary | std::ios::trunc);
      out << saved_index; }
    if (!tampered_catalog.open(report.bundle_root, &error) ||
        tampered_catalog.generated_subasset_count() != 2u)
        return test_failure(167, __LINE__);
    const auto cooked_model = runtime.assets().cooked_artifacts(model_guid);
    const auto cooked_material = runtime.assets().cooked_artifacts(material_guid);
    nengine::render::CookedModelMaterialMap generated_mappings;
    if (!cooked_model || !nengine::render::read_cooked_model_material_map(
            *cooked_model, generated_mappings) ||
        generated_mappings.size() != 1u ||
        generated_mappings.at(0u) != derived_material)
        return test_failure(163, __LINE__);
    if (!cooked_model || cooked_model->importer_id != "NEngine.Model" ||
        cooked_model->artifacts.size() < 2u ||
        cooked_model->artifacts[1].role != "model-descriptor" ||
        !cooked_material || cooked_material->artifacts.size() != 1u ||
        cooked_material->importer_id != "NEngine.Material")
        return test_failure(45, __LINE__);
    const auto cooked_texture = runtime.assets().cooked_artifacts(texture_guid);
    if (!cooked_texture || cooked_texture->artifacts.size() != 2u ||
        cooked_texture->artifacts[0].role != "source" ||
        cooked_texture->artifacts[1].role != "texture-descriptor" ||
        cooked_texture->importer_id != "NEngine.Texture")
        return test_failure(39, __LINE__);
    if (runtime.assets().cooked_artifacts(wav_guid)) return test_failure(40, __LINE__);
    { std::ifstream descriptor(cooked_texture->artifacts[1].path);
      std::string header; int version = 0;
      if (!(descriptor >> header >> version) ||
          header != "NENGINE_TEXTURE" || version != 1)
          return test_failure(41, __LINE__); }
    if (!decoded_ping || decoded_ping->sample_rate != 8000 ||
        decoded_ping->frame_count() != 1u) return test_failure(31, __LINE__);
    const auto sprite_clip = runtime.assets().load_sprite_clip(nanim_guid);
    if (!sprite_clip || sprite_clip->frames.size() != 2u ||
        !sprite_clip->valid() ||
        !runtime.assets().load_controller(controller_guid)) return test_failure(33, __LINE__);
    if (runtime.advance(std::numeric_limits<float>::quiet_NaN()).physics_steps ||
        runtime.advance(0.f).physics_steps) return test_failure(28, __LINE__);
    // The first valid host frame initializes the controller's Idle state.
    // The following half frame advances the 60 Hz physics accumulator.
    const auto initial_frame = runtime.advance(1.f / 120.f);
    if (initial_frame.physics_steps != 0 ||
        initial_frame.controller_transitions != 1u) return test_failure(28, __LINE__);
    const auto frame = runtime.advance(1.f / 120.f);
    if (frame.audio.advanced != 1u || frame.audio.looped != 1u ||
        frame.sprite_sampled != 1u || frame.controller_transitions != 0u ||
        !runtime.audio_clip(wav_guid)) return test_failure(32, __LINE__);
    bool sprite_visible = false;
    for (const auto entity : runtime.world().entities()) {
        const auto* sprite = runtime.world().get_component<
            nengine::render::SpriteRenderer>(
                entity, nengine::render::sprite_renderer_type());
        if (sprite && sprite->texture == texture_guid)
            sprite_visible = true;
    }
    if (!sprite_visible) return test_failure(35, __LINE__);
    for (const auto entity : runtime.world().entities()) {
        if (auto* animator = runtime.world().get_component<
                nengine::render::SpriteAnimator>(
                entity, nengine::render::sprite_animator_type()))
            animator->bool_parameters["moving"] = true;
    }
    const auto controller_frame = runtime.advance(0.02f);
    if (controller_frame.controller_transitions != 1u) return test_failure(37, __LINE__);
    bool running = false;
    for (const auto entity : runtime.world().entities()) {
        const auto* animator = runtime.world().get_component<
            nengine::render::SpriteAnimator>(
                entity, nengine::render::sprite_animator_type());
        if (animator && animator->clip == nanim_guid &&
            animator->speed == 2.0f) running = true;
    }
    if (!running) return test_failure(38, __LINE__);
    if (frame.physics_steps != 1 || frame.integrated_2d != 1 ||
        runtime.elapsed_seconds() <= 0.0)
        return test_failure(29, __LINE__);
    const auto time_before_pause = runtime.elapsed_seconds();
    runtime.set_paused(true);
    if (!runtime.paused() ||
        runtime.advance(1.f).physics_steps != 0 ||
        runtime.elapsed_seconds() != time_before_pause) return test_failure(36, __LINE__);
    runtime.set_paused(false);
    const auto long_frame = runtime.advance(2.f);
    if (!long_frame.clamped_delta || long_frame.physics_steps > 8)
        return test_failure(30, __LINE__);
    BundleManifest tampered = built;
    tampered.scene = "Content/Nonexistent.nscene";
    if (!save_bundle(tampered,
            report.bundle_root / "NEngine.player", &error)) return test_failure(18, __LINE__);
    if (runtime.load(report.bundle_root, &error) ||
        !runtime.ready() || runtime.entity_count() != 2u)
        return test_failure(19, __LINE__); // Failed reload must not partially overwrite old World.
    if (!save_bundle(built,
            report.bundle_root / "NEngine.player", &error)) return test_failure(20, __LINE__);
    std::filesystem::remove(
        report.bundle_root / "Content" / "Main.nscene");
    if (runtime.load(report.bundle_root, &error) ||
        !runtime.ready() || runtime.entity_count() != 2u)
        return test_failure(21, __LINE__);
    runtime.unload();
    if (runtime.ready() || runtime.entity_count() != 0 ||
        runtime.pending_script_count() != 0) return test_failure(14, __LINE__);
    { std::ofstream executable(root / "player.fixture", std::ios::binary);
      executable << "test executable"; }
    settings.destination = export_base / "RunnableBuild";
    settings.player_executable = root / "player.fixture";
    if (!export_player_data(settings, report, &error)) return test_failure(15, __LINE__);
#if defined(_WIN32)
    const auto packed_binary = report.bundle_root / "NEnginePlayer.exe";
#else
    const auto packed_binary = report.bundle_root / "NEnginePlayer";
#endif
    if (!std::filesystem::is_regular_file(packed_binary)) return test_failure(16, __LINE__);
    // A referenced external glTF buffer must be packaged, never silently
    // dropped; failure must not publish the destination directory.
    { std::ofstream gltf(project / "Assets" / "Models" / "broken.gltf");
      gltf << "{\"asset\":{\"version\":\"2.0\"},"
              "\"buffers\":[{\"uri\":\"missing.bin\",\"byteLength\":4}]}"; }
    { std::ofstream meta(project / "Assets" / "Models" / "broken.gltf.meta");
      meta << "NENGINE_META 1\nGUID "
           << nengine::assets::AssetGuid::generate().to_string()
           << "\nIMPORTER \"NEngine.Model\"\nEND_META\n"; }
    settings.destination = export_base / "MissingSidecar";
    if (export_player_data(settings, report, &error) ||
        std::filesystem::exists(settings.destination))
        return test_failure(46, __LINE__);
    std::filesystem::remove(project / "Assets" / "Models" / "broken.gltf");
    std::filesystem::remove(project / "Assets" / "Models" / "broken.gltf.meta");
    settings.destination = export_base / "MissingBinary";
    settings.player_executable = root / "missing-executable";
    if (export_player_data(settings, report, &error) ||
        std::filesystem::exists(settings.destination)) return test_failure(17, __LINE__);
    const auto managed_dir = root / "ManagedTest";
    std::filesystem::create_directory(managed_dir);
    for (const auto& filename : {"GameScripts.dll",
                                  "GameScripts.runtimeconfig.json",
                                  "NEngine.Bridge.dll", "NEngine.API.dll"}) {
        std::ofstream dll(managed_dir / filename);
        dll << "fixture";
    }
    { std::ofstream dll(managed_dir / "Unsafe.cs"); dll << "do not ship"; }
    settings.player_executable.clear();
    settings.managed_directory = managed_dir;
    settings.destination = export_base / "ManagedBuild";
    if (!export_player_data(settings, report, &error) ||
        report.managed_files != 4u ||
        !std::filesystem::is_regular_file(report.bundle_root / "Content" /
                                           "Managed" / "NEngine.Bridge.dll") ||
        std::filesystem::exists(report.bundle_root / "Content" /
                                "Managed" / "Unsafe.cs"))
        return test_failure(42, __LINE__);
    std::filesystem::remove(managed_dir / "NEngine.Bridge.dll");
    settings.destination = export_base / "BrokenManaged";
    if (export_player_data(settings, report, &error) ||
        std::filesystem::exists(settings.destination))
        return test_failure(43, __LINE__);
    settings.managed_directory.clear();
    settings.destination = export_base / "Build";
    if (export_player_data(settings, report, &error)) return test_failure(10, __LINE__);
    settings.destination = project / "BadExport";
    if (export_player_data(settings, report, &error) ||
        std::filesystem::exists(settings.destination)) return test_failure(11, __LINE__);
    settings.destination = export_base / "Bad";
    settings.startup_scene = "../outside.nscene";
    if (export_player_data(settings, report, &error) ||
        std::filesystem::exists(settings.destination)) return test_failure(12, __LINE__);
    settings.startup_scene = "Assets/Scenes/Main.nscene";
    settings.title.clear();
    settings.destination = export_base / "BadTitle";
    if (export_player_data(settings, report, &error) ||
        std::filesystem::exists(settings.destination)) return test_failure(26, __LINE__);
    settings.title = "Test Player";
    settings.destination = export_base / "MalformedScene";
    { std::ofstream invalid(project / "Assets" / "Scenes" / "Main.nscene",
                            std::ios::binary | std::ios::trunc);
      invalid << "INVALID_SCENE\n"; }
    if (export_player_data(settings, report, &error) ||
        std::filesystem::exists(settings.destination)) return test_failure(27, __LINE__);
    // Generated model subasset inventories are never parsed as arbitrary paths.
    const auto generated = root / "GeneratedSubassetTests";
    const auto generated_slot = generated / "subassets" / "material_0";
    std::filesystem::create_directories(generated_slot);
    const auto generated_material_guid = nengine::assets::AssetGuid::generate();
    const auto generated_texture_guid = nengine::assets::AssetGuid::generate();
    const auto generated_material_path = generated_slot / "material.nmat";
    const auto generated_texture_path = generated_slot / "base_color.tga";
    const auto generated_descriptor_path = generated_slot / "base_color.nasset";
    { std::ofstream out(generated_material_path); out << "NENGINE_MATERIAL 1\n"; }
    { std::ofstream out(generated_texture_path); out << "fixture"; }
    { std::ofstream out(generated_descriptor_path); out << "NENGINE_TEXTURE 1\n"; }
    nengine::assets::GeneratedSubasset generated_material;
    generated_material.guid = generated_material_guid;
    generated_material.importer_id = "NEngine.Material";
    generated_material.artifacts.push_back({generated_material_path, "source"});
    nengine::assets::GeneratedSubasset generated_texture;
    generated_texture.guid = generated_texture_guid;
    generated_texture.importer_id = "NEngine.Texture";
    generated_texture.artifacts.push_back({generated_texture_path, "source"});
    generated_texture.artifacts.push_back({generated_descriptor_path, "texture-descriptor"});
    std::vector<nengine::assets::GeneratedSubasset> generated_subassets{
        generated_material, generated_texture};
    std::vector<CookedSubasset> read_generated;
    if (!write_model_subassets(generated, generated_subassets, &error) ||
        !read_model_subassets(generated, read_generated, &error) ||
        read_generated.size() != 2u ||
        read_generated[0].guid != generated_material_guid ||
        read_generated[1].guid != generated_texture_guid ||
        read_generated[1].artifacts.size() != 2u)
        return test_failure(154, __LINE__);
    const auto generated_index = generated / "subassets.nasset";
    { std::ofstream out(generated_index, std::ios::trunc); out << "INVALID\n"; }
    const auto previous_generated = read_generated;
    if (read_model_subassets(generated, read_generated, &error) ||
        read_generated.size() != previous_generated.size() ||
        read_generated[0].guid != previous_generated[0].guid)
        return test_failure(155, __LINE__);
    if (!write_model_subassets(generated, generated_subassets, &error))
        return test_failure(156, __LINE__);
    auto bad_generated = generated_subassets;
    bad_generated[1].artifacts[0].path = generated / ".." / "escape.tga";
    if (write_model_subassets(generated, bad_generated, &error))
        return test_failure(157, __LINE__);
    bad_generated = generated_subassets;
    bad_generated[1].artifacts[0].path = generated_material_path;
    if (write_model_subassets(generated, bad_generated, &error))
        return test_failure(158, __LINE__);
    bad_generated = generated_subassets;
    bad_generated[1].guid = generated_material_guid;
    if (write_model_subassets(generated, bad_generated, &error))
        return test_failure(159, __LINE__);
    bad_generated = generated_subassets;
    bad_generated[1].importer_id = "NEngine.GameCode";
    if (write_model_subassets(generated, bad_generated, &error))
        return test_failure(160, __LINE__);
    bad_generated = generated_subassets;
    bad_generated[1].artifacts[1].role = "source";
    if (write_model_subassets(generated, bad_generated, &error))
        return test_failure(161, __LINE__);
    // GENERATED_SUBASSET_TESTS
    std::filesystem::remove_all(root);
    std::cout << "player bundle export tests passed\n";
    return 0;
}
