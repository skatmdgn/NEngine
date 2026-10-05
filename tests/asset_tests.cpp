#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "nengine/assets/asset_database.hpp"
#include "nengine/assets/asset_guid.hpp"
#include "nengine/assets/asset_importer.hpp"
#include "nengine/assets/dependency_graph.hpp"
#include "nengine/assets/file_watcher.hpp"

namespace {
int failures = 0;

void check(bool condition, const char* message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

void write_file(const std::filesystem::path& path, std::string_view content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << content;
}
}

int main() {
    using namespace nengine::assets;

    const auto generated = AssetGuid::generate();
    check(generated.valid(), "generated GUID is valid");
    const auto parsed = AssetGuid::parse(generated.to_string());
    check(parsed.has_value() && *parsed == generated, "GUID string roundtrip");

    ImporterRegistry importers;
    check(importers.register_importer({"Texture", 1, {".png", ".jpg"}, false}), "register texture importer");
    check(importers.register_importer({"Scene", 1, {".nscene"}, false}), "register scene importer");
    check(importers.register_importer({"Raw", 1, {}, true}), "register fallback importer");
    check(importers.find_for_path("image.PNG")->id == "Texture", "extension matching is case-insensitive");
    check(importers.find_for_path("unknown.bin")->id == "Raw", "fallback importer resolves unknown files");

    const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    const auto root =
        std::filesystem::temp_directory_path() /
        ("nengine_asset_test_" + std::to_string(stamp));

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root);

    const auto image = root / "Textures" / "hero.png";
    const auto scene = root / "Scenes" / "Main.nscene";
    write_file(image, "png-data");
    write_file(scene, "scene-data");

    AssetDatabase database(root);
    database.set_importers(&importers);

    const auto first_scan = database.scan(true);
    check(database.size() == 2, "database scans asset files");
    check(first_scan.meta_created == 2, "missing meta files are created");

    const auto* image_record = database.find_relative("Textures/hero.png");
    check(image_record != nullptr, "asset lookup by relative path");
    check(image_record && image_record->importer_id == "Texture", "asset chooses registered importer");
    check(std::filesystem::exists(image.string() + ".meta"), "image meta persisted");

    const auto original_guid = image_record ? image_record->guid : AssetGuid{};
    const auto second_scan = database.scan(true);
    const auto* image_after_rescan = database.find_relative("Textures/hero.png");
    check(second_scan.added == 0, "rescan does not duplicate assets");
    check(image_after_rescan && image_after_rescan->guid == original_guid, "rescan preserves GUID");

    const auto moved = root / "Textures" / "Characters" / "hero.png";
    std::filesystem::create_directories(moved.parent_path());
    std::filesystem::rename(image, moved);
    std::filesystem::rename(
        std::filesystem::path{image.string() + ".meta"},
        std::filesystem::path{moved.string() + ".meta"});

    database.scan(true);
    const auto* moved_record = database.find_relative("Textures/Characters/hero.png");
    check(moved_record && moved_record->guid == original_guid, "moving asset with meta preserves identity");
    check(database.find_relative("Textures/hero.png") == nullptr, "old path is removed after move");

    PollingFileWatcher watcher(root);
    check(watcher.poll().empty(), "first watcher poll establishes baseline");

    const auto script = root / "Scripts" / "Player.cs";
    write_file(script, "class Player {}");
    auto changes = watcher.poll();
    check(changes.size() == 1 && changes[0].kind == FileChangeKind::Added, "watcher detects added file");

    write_file(script, "class Player { int hp; }");
    changes = watcher.poll();
    check(changes.size() == 1 && changes[0].kind == FileChangeKind::Modified, "watcher detects modified file");

    std::filesystem::remove(script);
    changes = watcher.poll();
    check(changes.size() == 1 && changes[0].kind == FileChangeKind::Removed, "watcher detects removed file");

    const auto a = AssetGuid::generate();
    const auto b = AssetGuid::generate();
    const auto c = AssetGuid::generate();

    AssetDependencyGraph graph;
    graph.set_dependencies(a, {b, c});
    check(graph.dependencies(a).size() == 2, "dependency graph stores forward edges");
    check(graph.dependents(b).size() == 1 && graph.dependents(b)[0] == a, "dependency graph stores reverse edges");
    graph.remove(b);
    check(graph.dependencies(a).size() == 1 && graph.dependencies(a)[0] == c, "dependency removal cleans reverse and forward edges");

    std::filesystem::remove_all(root, ec);

    if (failures == 0) {
        std::cout << "NEngineAssetTests: PASS\n";
        return EXIT_SUCCESS;
    }

    std::cerr << "NEngineAssetTests: " << failures << " failure(s)\n";
    return EXIT_FAILURE;
}
