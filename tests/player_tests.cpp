#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include "nengine/player/bundle.hpp"
#include "nengine/player/exporter.hpp"
#include "nengine/player/runtime.hpp"
#include "nengine/core/scene.hpp"

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
    world.create("Camera");
    world.create("Sprite");
    auto scene = nengine::core::SceneSerializer::capture(world, "Main");
    if (!nengine::core::SceneSerializer::save_file(
        scene, project / "Assets" / "Scenes" / "Main.nscene")) return 7;
    ExportSettings settings;
    settings.project_root = project;
    settings.startup_scene = "Assets/Scenes/Main.nscene";
    settings.destination = export_base / "Build";
    settings.title = "Test Player";
    ExportReport report;
    if (!export_player_data(settings, report, &error) ||
        report.scene_objects != 2u ||
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
        !runtime.ready() || runtime.entity_count() != 2u) return 13;
    runtime.unload();
    if (runtime.ready() || runtime.entity_count() != 0) return 14;
    if (export_player_data(settings, report, &error)) return 10;
    settings.destination = project / "BadExport";
    if (export_player_data(settings, report, &error) ||
        std::filesystem::exists(settings.destination)) return 11;
    settings.destination = export_base / "Bad";
    settings.startup_scene = "../outside.nscene";
    if (export_player_data(settings, report, &error) ||
        std::filesystem::exists(settings.destination)) return 12;
    std::filesystem::remove_all(root);
    std::cout << "player bundle export tests passed\n";
    return 0;
}
