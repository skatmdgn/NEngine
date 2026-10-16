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
#include "nengine/core/scene.hpp"
#include "nengine/render/components.hpp"
#include "nengine/render/registration.hpp"
#include "nengine/audio/components.hpp"
#include "nengine/audio/registration.hpp"
#include "nengine/physics/components.hpp"
#include "nengine/physics/registration.hpp"
#include "nengine/scripting/components.hpp"
#include "nengine/scripting/registration.hpp"

int main() {
    using namespace nengine::player;
    BundleManifest manifest;
    manifest.title = "Demo";
    std::stringstream stream;
    if (!write_bundle(stream, manifest)) return 1;
    BundleManifest restored;
    if (!read_bundle(stream, restored) ||
        restored.title != manifest.title ||
        restored.scene != manifest.scene) return 2;
    restored.scene = "../outside.nscene";
    if (validate_bundle(restored)) return 3;
    restored = manifest;
    restored.width = 0;
    if (validate_bundle(restored)) return 4;
    const auto root = std::filesystem::temp_directory_path() /
        "nengine_player_bundle_test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "Content");
    { std::ofstream file(root / "Content" / "Main.nscene"); file << "fixture"; }
    std::string error;
    if (!resolve_bundle_scene(root, manifest, &error)) return 5;
    if (resolve_bundle_scene(root / "missing", manifest, &error)) return 6;
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
    auto* sound = world.add_component<nengine::audio::AudioSource>(
        body_entity, nengine::audio::audio_source_type());
    if (!camera || !body || !sound) return 22;
    camera->vertical_fov_degrees = 71.f;
    body->mass = 2.5f;
    sound->volume = 0.6f;
    sound->clip = wav_guid;
    sound->loop = true;
    auto* behaviour = world.add_component<nengine::scripting::ScriptBehaviour>(
        body_entity, nengine::scripting::script_behaviour_type());
    if (!behaviour) return 25;
    behaviour->type_name = "Game.Player";
    nengine::core::ComponentSerializationRegistry codecs;
    if (!nengine::render::register_component_serializers(codecs) ||
        !nengine::physics::register_component_serializers(codecs) ||
        !nengine::audio::register_component_serializers(codecs) ||
        !nengine::scripting::register_component_serializers(codecs)) return 23;
    auto scene = nengine::core::SceneSerializer::capture(world, "Main", &codecs);
    if (!nengine::core::SceneSerializer::save_file(
        scene, project / "Assets" / "Scenes" / "Main.nscene")) return 7;
    std::filesystem::create_directories(project / "Assets" / "Textures");
    { std::ofstream file(project / "Assets" / "Textures" / "hero.png");
      file << "texture test bytes"; }
    const auto texture_guid = nengine::assets::AssetGuid::generate();
    { std::ofstream meta(project / "Assets" / "Textures" / "hero.png.meta");
      meta << "NENGINE_META 1\nGUID " << texture_guid.to_string()
           << "\nIMPORTER " << std::quoted(std::string("NEngine.Texture"))
           << "\nEND_META\n"; }
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
    { std::ofstream file(project / "Assets" / "Textures" / "ignore.cs");
      file << "do not ship source code"; }
    ExportSettings settings;
    settings.project_root = project;
    settings.startup_scene = "Assets/Scenes/Main.nscene";
    settings.destination = export_base / "Build";
    settings.title = "Test Player";
    ExportReport report;
    if (!export_player_data(settings, report, &error) ||
        report.scene_objects != 2u ||
        report.copied_assets < 2u ||
        !std::filesystem::exists(
            report.bundle_root / "Content" / "Assets" / "Textures" / "hero.png") ||
        std::filesystem::exists(
            report.bundle_root / "Content" / "Assets" / "Textures" / "ignore.cs") ||
        !std::filesystem::exists(
            report.bundle_root / "Content" / "Main.nscene"))
        return 8;
    BundleManifest built;
    if (!load_bundle(report.bundle_root / "NEngine.player", built, &error) ||
        built.title != settings.title ||
        !resolve_bundle_scene(report.bundle_root, built, &error))
        return 9;
    PlayerRuntime runtime;
    if (!runtime.load(report.bundle_root, &error) ||
        !runtime.ready() || runtime.entity_count() != 2u ||
        runtime.pending_script_count() != 1u ||
        !runtime.assets().source(texture_guid)) return 13;
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
    if (!restored_camera || !restored_body) return 24;
    const auto decoded_ping = runtime.assets().load_wav(wav_guid);
    if (!decoded_ping || decoded_ping->sample_rate != 8000 ||
        decoded_ping->frame_count() != 1u) return 31;
    const auto sprite_clip = runtime.assets().load_sprite_clip(nanim_guid);
    if (!sprite_clip || sprite_clip->frames.size() != 2u ||
        !sprite_clip->valid()) return 33;
    if (runtime.advance(std::numeric_limits<float>::quiet_NaN()).physics_steps ||
        runtime.advance(0.f).physics_steps ||
        runtime.advance(1.f / 120.f).physics_steps != 0)
        return 28;
    const auto frame = runtime.advance(1.f / 120.f);
    if (frame.audio.advanced != 1u || frame.audio.looped != 1u ||
        !runtime.audio_clip(wav_guid)) return 32;
    if (frame.physics_steps != 1 || frame.integrated_2d != 1 ||
        runtime.elapsed_seconds() <= 0.0)
        return 29;
    const auto long_frame = runtime.advance(2.f);
    if (!long_frame.clamped_delta || long_frame.physics_steps > 8)
        return 30;
    BundleManifest tampered = built;
    tampered.scene = "Content/Nonexistent.nscene";
    if (!save_bundle(tampered,
            report.bundle_root / "NEngine.player", &error)) return 18;
    if (runtime.load(report.bundle_root, &error) ||
        !runtime.ready() || runtime.entity_count() != 2u)
        return 19; // Failed reload must not partially overwrite old World.
    if (!save_bundle(built,
            report.bundle_root / "NEngine.player", &error)) return 20;
    std::filesystem::remove(
        report.bundle_root / "Content" / "Main.nscene");
    if (runtime.load(report.bundle_root, &error) ||
        !runtime.ready() || runtime.entity_count() != 2u)
        return 21;
    runtime.unload();
    if (runtime.ready() || runtime.entity_count() != 0 ||
        runtime.pending_script_count() != 0) return 14;
    { std::ofstream executable(root / "player.fixture", std::ios::binary);
      executable << "test executable"; }
    settings.destination = export_base / "RunnableBuild";
    settings.player_executable = root / "player.fixture";
    if (!export_player_data(settings, report, &error)) return 15;
#if defined(_WIN32)
    const auto packed_binary = report.bundle_root / "NEnginePlayer.exe";
#else
    const auto packed_binary = report.bundle_root / "NEnginePlayer";
#endif
    if (!std::filesystem::is_regular_file(packed_binary)) return 16;
    settings.destination = export_base / "MissingBinary";
    settings.player_executable = root / "missing-executable";
    if (export_player_data(settings, report, &error) ||
        std::filesystem::exists(settings.destination)) return 17;
    settings.player_executable.clear();
    settings.destination = export_base / "Build";
    if (export_player_data(settings, report, &error)) return 10;
    settings.destination = project / "BadExport";
    if (export_player_data(settings, report, &error) ||
        std::filesystem::exists(settings.destination)) return 11;
    settings.destination = export_base / "Bad";
    settings.startup_scene = "../outside.nscene";
    if (export_player_data(settings, report, &error) ||
        std::filesystem::exists(settings.destination)) return 12;
    settings.startup_scene = "Assets/Scenes/Main.nscene";
    settings.title.clear();
    settings.destination = export_base / "BadTitle";
    if (export_player_data(settings, report, &error) ||
        std::filesystem::exists(settings.destination)) return 26;
    settings.title = "Test Player";
    settings.destination = export_base / "MalformedScene";
    { std::ofstream invalid(project / "Assets" / "Scenes" / "Main.nscene",
                            std::ios::binary | std::ios::trunc);
      invalid << "INVALID_SCENE\n"; }
    if (export_player_data(settings, report, &error) ||
        std::filesystem::exists(settings.destination)) return 27;
    std::filesystem::remove_all(root);
    std::cout << "player bundle export tests passed\n";
    return 0;
}
