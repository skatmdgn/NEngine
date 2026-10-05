#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "nengine/assets/asset_database.hpp"
#include "nengine/assets/asset_guid.hpp"
#include "nengine/assets/asset_importer.hpp"
#include "nengine/assets/builtin_processors.hpp"
#include "nengine/assets/dependency_graph.hpp"
#include "nengine/assets/file_watcher.hpp"
#include "nengine/assets/import_pipeline.hpp"

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
    output.write(
        content.data(),
        static_cast<std::streamsize>(
            content.size()));
}

void write_bytes(
    const std::filesystem::path& path,
    const std::vector<unsigned char>& bytes) {

    std::filesystem::create_directories(
        path.parent_path());

    std::ofstream output(
        path,
        std::ios::binary |
            std::ios::trunc);

    output.write(
        reinterpret_cast<
            const char*>(
                bytes.data()),
        static_cast<std::streamsize>(
            bytes.size()));
}

std::string read_all(
    const std::filesystem::path& path) {

    std::ifstream input(
        path,
        std::ios::binary);

    return {
        std::istreambuf_iterator<char>{
            input},
        std::istreambuf_iterator<char>{}
    };
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
    check(importers.register_importer({"Audio", 1, {".wav"}, false}), "register audio importer");
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
    const auto audio = root / "Audio" / "tone.wav";
    const auto scene = root / "Scenes" / "Main.nscene";

    // Minimal PNG signature + IHDR header. CRC/data are not
    // required by the metadata-only importer.
    write_bytes(
        image,
        {
            0x89, 'P', 'N', 'G',
            0x0D, 0x0A, 0x1A, 0x0A,
            0x00, 0x00, 0x00, 0x0D,
            'I', 'H', 'D', 'R',
            0x00, 0x00, 0x00, 0x40,
            0x00, 0x00, 0x00, 0x20
        });

    // 44-byte PCM WAV header + one stereo 16-bit frame.
    write_bytes(
        audio,
        {
            'R','I','F','F',
            40,0,0,0,
            'W','A','V','E',
            'f','m','t',' ',
            16,0,0,0,
            1,0,
            2,0,
            0x44,0xAC,0x00,0x00,
            0x10,0xB1,0x02,0x00,
            4,0,
            16,0,
            'd','a','t','a',
            4,0,0,0,
            0,0,0,0
        });

    write_file(scene, "scene-data");

    AssetDatabase database(root);
    database.set_importers(&importers);

    const auto first_scan = database.scan(true);
    check(database.size() == 3, "database scans asset files");
    check(first_scan.meta_created == 3, "missing meta files are created");

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

    AssetImportPipeline pipeline;
    check(
        pipeline.register_processor(
            "Scene",
            copy_source_importer),
        "scene copy-source processor registers");

    check(
        pipeline.register_processor(
            "Texture",
            texture_source_importer),
        "texture metadata processor registers");

    check(
        pipeline.register_processor(
            "Audio",
            audio_source_importer),
        "audio metadata processor registers");

    check(
        pipeline.register_processor(
            "Raw",
            copy_source_importer),
        "raw copy-source processor registers");

    const auto* scene_record =
        database.find_relative("Scenes/Main.nscene");

    check(
        scene_record != nullptr,
        "scene record available for import pipeline");

    const auto cache_root =
        root / "Library" / "Cache";

    auto import_result =
        pipeline.import(
            *scene_record,
            importers,
            cache_root);

    check(
        import_result.success &&
        !import_result.cache_hit,
        "first supported import creates artifact");

    check(
        import_result.artifacts.size() == 1 &&
        std::filesystem::exists(
            import_result.artifacts[0].path),
        "import artifact is written to cache");

    const auto* texture_record =
        database.find_relative(
            "Textures/Characters/hero.png");

    check(
        texture_record != nullptr,
        "moved texture record available for import");

    const auto texture_result =
        pipeline.import(
            *texture_record,
            importers,
            cache_root);

    check(
        texture_result.success &&
        texture_result.artifacts.size() == 2,
        "texture importer produces source and descriptor");

    std::filesystem::path texture_descriptor;

    for (const auto& artifact :
         texture_result.artifacts) {
        if (artifact.role ==
            "texture-descriptor") {
            texture_descriptor =
                artifact.path;
        }
    }

    const auto texture_text =
        read_all(
            texture_descriptor);

    check(
        texture_text.find("WIDTH 64") !=
            std::string::npos &&
        texture_text.find("HEIGHT 32") !=
            std::string::npos,
        "texture descriptor records PNG dimensions");

    const auto* audio_record =
        database.find_relative(
            "Audio/tone.wav");

    check(
        audio_record != nullptr,
        "audio record available for import");

    const auto audio_result =
        pipeline.import(
            *audio_record,
            importers,
            cache_root);

    check(
        audio_result.success &&
        audio_result.artifacts.size() == 2,
        "audio importer produces source and descriptor");

    std::filesystem::path audio_descriptor;

    for (const auto& artifact :
         audio_result.artifacts) {
        if (artifact.role ==
            "audio-descriptor") {
            audio_descriptor =
                artifact.path;
        }
    }

    const auto audio_text =
        read_all(
            audio_descriptor);

    check(
        audio_text.find("CHANNELS 2") !=
            std::string::npos &&
        audio_text.find("SAMPLE_RATE 44100") !=
            std::string::npos &&
        audio_text.find("BITS_PER_SAMPLE 16") !=
            std::string::npos,
        "audio descriptor records WAV metadata");

    const auto raw_record =
        AssetRecord{
            scene_record->guid,
            scene_record->source_path,
            scene_record->relative_path,
            scene_record->meta_path,
            "Raw",
            scene_record->file_size,
            scene_record->write_stamp
        };

    import_result =
        pipeline.import(
            raw_record,
            importers,
            cache_root);

    check(
        import_result.success &&
        !import_result.cache_hit,
        "changing importer invalidates cache fingerprint");

    const auto cached_result =
        pipeline.import(
            raw_record,
            importers,
            cache_root);

    check(
        cached_result.success &&
        cached_result.cache_hit,
        "unchanged import resolves from cache manifest");

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
