#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

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
    check(importers.register_importer({"Shader", 1, {".spv"}, false}), "register shader importer");
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
    const auto shader = root / "Shaders" / "basic.vert.spv";
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

    // Minimal SPIR-V module header (5 words). The importer
    // validates binary identity/stage hint; semantic shader
    // validation belongs to Vulkan shader-module creation/tooling.
    write_bytes(
        shader,
        {
            0x03, 0x02, 0x23, 0x07,
            0x00, 0x00, 0x01, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x01, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00
        });

    write_file(scene, "scene-data");

    AssetDatabase database(root);
    database.set_importers(&importers);

    const auto first_scan = database.scan(true);
    check(database.size() == 4, "database scans asset files");
    check(first_scan.meta_created == 4, "missing meta files are created");

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
            "Shader",
            shader_source_importer),
        "SPIR-V shader processor registers");

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

    const auto* shader_record =
        database.find_relative(
            "Shaders/basic.vert.spv");

    check(
        shader_record != nullptr &&
        shader_record->importer_id ==
            "Shader",
        "SPIR-V file resolves to shader importer");

    const auto shader_result =
        pipeline.import(
            *shader_record,
            importers,
            cache_root);

    check(
        shader_result.success &&
        shader_result.artifacts.size() == 2,
        "SPIR-V shader importer produces source and descriptor");

    std::filesystem::path shader_descriptor;

    for (const auto& artifact :
         shader_result.artifacts) {

        if (artifact.role ==
            "shader-descriptor") {
            shader_descriptor =
                artifact.path;
        }
    }

    const auto shader_text =
        read_all(
            shader_descriptor);

    check(
        shader_text.find(
            "STAGE \"vertex\"") !=
            std::string::npos &&
        shader_text.find(
            "WORDS 5") !=
            std::string::npos,
        "shader descriptor records stage hint and word count");

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

    const auto cached_artifacts =
        pipeline.cached_artifacts(
            raw_record,
            importers,
            cache_root);

    check(
        cached_artifacts.has_value() &&
        !cached_artifacts->artifacts.empty(),
        "validated cache lookup returns current artifacts");

    const auto generated_subasset_guid =
        AssetGuid::generate();

    AssetImportPipeline
        generated_pipeline;

    check(
        generated_pipeline.register_processor(
            "Raw",
            [generated_subasset_guid](
                const ImportContext& context) {

                ImportResult result;

                const auto generated_source =
                    context.cache_directory /
                    "generated" /
                    "material.nmat";

                std::error_code local_error;

                std::filesystem::create_directories(
                    generated_source.parent_path(),
                    local_error);

                if (local_error) {
                    result.message =
                        "generated subasset directory failed";
                    return result;
                }

                write_file(
                    generated_source,
                    "NENGINE_MATERIAL 1\n"
                    "BASE_COLOR_TEXTURE "
                    "\"11111111111111112222222222222222\"\n"
                    "END_MATERIAL\n");

                result.success = true;
                result.message =
                    "generated subasset test import";

                GeneratedSubasset subasset;
                subasset.guid =
                    generated_subasset_guid;
                subasset.importer_id =
                    "NEngine.Material";
                subasset.name =
                    "Generated Material";
                subasset.artifacts.push_back({
                    generated_source,
                    "source"
                });

                result.subassets.push_back(
                    std::move(
                        subasset));

                return result;
            }),
        "generated-subasset test processor registers");

    const auto generated_cache_root =
        root /
        "Library" /
        "GeneratedCache";

    const auto generated_first =
        generated_pipeline.import(
            raw_record,
            importers,
            generated_cache_root);

    check(
        generated_first.success &&
        !generated_first.cache_hit &&
        generated_first.subassets.size() == 1u &&
        generated_first.subassets[0].guid ==
            generated_subasset_guid &&
        generated_first.subassets[0].valid(),
        "import pipeline returns generated subasset on first import");

    const auto generated_cached =
        generated_pipeline.import(
            raw_record,
            importers,
            generated_cache_root);

    check(
        generated_cached.success &&
        generated_cached.cache_hit &&
        generated_cached.subassets.size() == 1u &&
        generated_cached.subassets[0].guid ==
            generated_subasset_guid &&
        std::filesystem::exists(
            generated_cached
                .subassets[0]
                .artifacts[0]
                .path),
        "import manifest v3 restores generated subassets on cache hit");

    const auto generated_artifacts =
        generated_pipeline
            .cached_subasset_artifacts(
                raw_record,
                generated_subasset_guid,
                importers,
                generated_cache_root);

    check(
        generated_artifacts.has_value() &&
        generated_artifacts->importer_id ==
            "NEngine.Material" &&
        generated_artifacts->artifacts.size() == 1u &&
        generated_artifacts->artifacts[0].role ==
            "source" &&
        std::filesystem::exists(
            generated_artifacts
                ->artifacts[0]
                .path),
        "generated subasset GUID resolves its independent cached artifact set");

    const auto retained_guid =
        AssetGuid::generate();

    const auto stale_guid =
        AssetGuid::generate();

    bool emit_stale_subasset =
        true;

    AssetImportPipeline
        cleanup_pipeline;

    check(
        cleanup_pipeline.register_processor(
            "Raw",
            [&](const ImportContext& context) {

                ImportResult result;
                result.success = true;

                const auto retained_path =
                    context.cache_directory /
                    "generated" /
                    "retained.bin";

                write_file(
                    retained_path,
                    "retained");

                GeneratedSubasset retained;
                retained.guid =
                    retained_guid;
                retained.importer_id =
                    "Generated.Binary";
                retained.name =
                    "Retained";
                retained.artifacts.push_back({
                    retained_path,
                    "source"
                });

                result.subassets.push_back(
                    std::move(
                        retained));

                if (emit_stale_subasset) {
                    const auto stale_path =
                        context.cache_directory /
                        "generated" /
                        "stale.bin";

                    write_file(
                        stale_path,
                        "stale");

                    GeneratedSubasset stale;
                    stale.guid =
                        stale_guid;
                    stale.importer_id =
                        "Generated.Binary";
                    stale.name =
                        "Stale";
                    stale.artifacts.push_back({
                        stale_path,
                        "source"
                    });

                    result.subassets.push_back(
                        std::move(
                            stale));
                }

                return result;
            }),
        "stale generated artifact cleanup processor registers");

    auto cleanup_record =
        raw_record;

    cleanup_record.write_stamp =
        raw_record.write_stamp +
        1000;

    const auto cleanup_cache_root =
        root /
        "Library" /
        "CleanupCache";

    const auto cleanup_first =
        cleanup_pipeline.import(
            cleanup_record,
            importers,
            cleanup_cache_root);

    std::filesystem::path
        stale_artifact_path;

    std::filesystem::path
        retained_artifact_path;

    for (const auto& subasset :
         cleanup_first.subassets) {

        if (subasset.guid ==
            stale_guid) {
            stale_artifact_path =
                subasset.artifacts[0]
                    .path;
        }

        if (subasset.guid ==
            retained_guid) {
            retained_artifact_path =
                subasset.artifacts[0]
                    .path;
        }
    }

    check(
        cleanup_first.success &&
        cleanup_first.subassets.size() == 2u &&
        std::filesystem::exists(
            stale_artifact_path) &&
        std::filesystem::exists(
            retained_artifact_path),
        "first generated-subasset import writes retained and soon-stale artifacts");

    // Cached manifest records are untrusted. Oversized counts must not
    // reserve huge memory and must trigger a safe cache miss.
    const auto cleanup_dir =
        cleanup_cache_root / cleanup_record.guid.to_string();
    const auto cleanup_manifest = cleanup_dir / "import.meta";
    const auto original_manifest = read_all(cleanup_manifest);
    const std::string empty_artifacts = "ARTIFACTS 0\n";
    const auto count_offset = original_manifest.find(empty_artifacts);
    check(count_offset != std::string::npos,
          "generated import manifest has no root artifacts");
    if (count_offset != std::string::npos) {
        auto oversized_manifest = original_manifest;
        oversized_manifest.replace(count_offset, empty_artifacts.size(),
            "ARTIFACTS 999999999\n");
        write_file(cleanup_manifest, oversized_manifest);
        check(!cleanup_pipeline.cached_artifacts(
                cleanup_record, importers, cleanup_cache_root),
              "oversized cached artifact count is rejected before reserve");
        write_file(cleanup_manifest, original_manifest);

        const auto external = root / "OutsideCache";
        const auto victim = external / "victim.bin";
        write_file(victim, "do-not-delete");
        const auto cache_link = cleanup_dir / "external-link";
        std::error_code symlink_error;
        std::filesystem::create_directory_symlink(
            external, cache_link, symlink_error);
        if (!symlink_error) {
            auto malicious = original_manifest;
            malicious.replace(count_offset, empty_artifacts.size(),
                "ARTIFACTS 1\nARTIFACT \"source\" \"external-link/victim.bin\"\n");
            write_file(cleanup_manifest, malicious);
            check(!cleanup_pipeline.cached_artifacts(
                    cleanup_record, importers, cleanup_cache_root),
                  "cache symlink escape is rejected on lookup");
            // The subsequent re-import runs stale artifact cleanup.
            // A vulnerable implementation would remove external/victim.bin.
        }
    }

    emit_stale_subasset = false;
    ++cleanup_record.write_stamp;

    const auto cleanup_second =
        cleanup_pipeline.import(
            cleanup_record,
            importers,
            cleanup_cache_root);

    check(
        cleanup_second.success &&
        !cleanup_second.cache_hit &&
        cleanup_second.subassets.size() == 1u &&
        cleanup_second.subassets[0].guid ==
            retained_guid &&
        std::filesystem::exists(
            retained_artifact_path) &&
        !std::filesystem::exists(
            stale_artifact_path),
        "reimport removes artifacts no longer referenced by root/subasset manifest while retaining current files");

    check(read_all(root / "OutsideCache" / "victim.bin") == "do-not-delete",
          "cache cleanup never deletes an external file through a symlink");

    write_file(
        scene,
        "scene-data-changed");

    database.scan(true);

    const auto* changed_scene =
        database.find_relative(
            "Scenes/Main.nscene");

    check(
        changed_scene != nullptr,
        "changed source remains in asset database");

    check(
        changed_scene &&
        !pipeline.cached_artifacts(
            *changed_scene,
            importers,
            cache_root).has_value(),
        "source fingerprint change invalidates cached artifact lookup");

    const auto a = AssetGuid::generate();
    const auto b = AssetGuid::generate();
    const auto c = AssetGuid::generate();

    AssetDependencyGraph graph;
    graph.set_dependencies(a, {b, c});
    check(graph.dependencies(a).size() == 2, "dependency graph stores forward edges");
    check(graph.dependents(b).size() == 1 && graph.dependents(b)[0] == a, "dependency graph stores reverse edges");
    graph.remove(b);
    check(graph.dependencies(a).size() == 1 && graph.dependencies(a)[0] == c, "dependency removal cleans reverse and forward edges");

    // Verification scans must not mutate GUID identity or .meta files.
    const auto duplicate_root = root / "DuplicatedGuids";
    const auto asset_a = duplicate_root / "a.bin";
    const auto asset_b = duplicate_root / "b.bin";
    write_file(asset_a, "a");
    write_file(asset_b, "b");
    const auto reused_guid = AssetGuid::generate().to_string();
    const auto duplicate_meta =
        "NENGINE_META 1\nGUID " + reused_guid +
        "\nIMPORTER \"Raw\"\nEND_META\n";
    write_file(std::filesystem::path(asset_a.string() + ".meta"), duplicate_meta);
    write_file(std::filesystem::path(asset_b.string() + ".meta"), duplicate_meta);
    AssetDatabase readonly_database(duplicate_root);
    const auto readonly_result = readonly_database.scan(false);
    bool has_collision_error = false;
    for (const auto& message : readonly_result.messages)
        has_collision_error |= message.severity == AssetMessageSeverity::Error;
    check(has_collision_error && readonly_database.size() == 1u &&
              read_all(std::filesystem::path(asset_a.string() + ".meta")) == duplicate_meta &&
              read_all(std::filesystem::path(asset_b.string() + ".meta")) == duplicate_meta,
          "read-only duplicate GUID scan emits error without changing metadata");
    const auto repaired_result = readonly_database.scan(true);
    check(readonly_database.size() == 2u && repaired_result.meta_repaired > 0,
          "mutable Editor scan can explicitly repair GUID collisions");

    const auto huge_root = root / "HugeMetadata";
    const auto huge_asset = huge_root / "huge.bin";
    write_file(huge_asset, "x");
    const auto huge_meta = std::filesystem::path(huge_asset.string() + ".meta");
    write_file(huge_meta, "NENGINE_META 1\nGUID " +
                          AssetGuid::generate().to_string() +
                          "\nIMPORTER \"" + std::string(9000u, 'X'));
    AssetDatabase bounded_database(huge_root);
    const auto bounded_scan = bounded_database.scan(false);
    check(bounded_database.size() == 0u &&
          !bounded_scan.messages.empty() &&
          std::filesystem::file_size(huge_meta) > 8192u,
          "oversized .meta is rejected in read-only scan");

    std::filesystem::remove_all(root, ec);

    if (failures == 0) {
        std::cout << "NEngineAssetTests: PASS\n";
        return EXIT_SUCCESS;
    }

    std::cerr << "NEngineAssetTests: " << failures << " failure(s)\n";
    return EXIT_FAILURE;
}
