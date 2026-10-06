#include "nengine/editor/project_session.hpp"

#include <deque>
#include <fstream>
#include <unordered_set>

#include "nengine/assets/builtin_processors.hpp"
#include "nengine/assets/gltf_sidecars.hpp"
#include "nengine/core/scene.hpp"
#include "nengine/render/builtin_assets.hpp"
#include "nengine/render/components.hpp"
#include "nengine/render/registration.hpp"
#include "nengine/render/material_asset.hpp"
#include <system_error>
#include <utility>

namespace nengine::editor {

ProjectSession::ProjectSession() {
    register_builtin_importers();

    import_pipeline_.register_processor(
        "NEngine.Scene",
        assets::copy_source_importer);

    import_pipeline_.register_processor(
        "NEngine.Script",
        assets::copy_source_importer);

    import_pipeline_.register_processor(
        "NEngine.Texture",
        assets::texture_source_importer);

    import_pipeline_.register_processor(
        "NEngine.Material",
        assets::material_source_importer);

    import_pipeline_.register_processor(
        "NEngine.Model",
        assets::model_source_importer);

    import_pipeline_.register_processor(
        "NEngine.Audio",
        assets::audio_source_importer);

    import_pipeline_.register_processor(
        "NEngine.Shader",
        assets::shader_source_importer);

    import_pipeline_.register_processor(
        "NEngine.Raw",
        assets::copy_source_importer);
}

void ProjectSession::register_builtin_importers() {
    importers_.register_importer({
        "NEngine.Scene", 1, {".nscene"}, false
    });

    importers_.register_importer({
        "NEngine.Script", 1, {".cs"}, false
    });

    importers_.register_importer({
        "NEngine.Texture", 1,
        {".png", ".jpg", ".jpeg", ".webp", ".bmp", ".tga"},
        false
    });

    importers_.register_importer({
        "NEngine.Material", 1,
        {".nmat"},
        false
    });

    importers_.register_importer({
        "NEngine.Model", 1,
        {".gltf", ".glb", ".obj", ".fbx"},
        false
    });

    importers_.register_importer({
        "NEngine.Audio", 1,
        {".wav", ".ogg", ".mp3", ".flac"},
        false
    });

    importers_.register_importer({
        "NEngine.Shader", 1,
        {".spv"},
        false
    });

    importers_.register_importer({
        "NEngine.Raw", 1, {}, true
    });
}

bool ProjectSession::open(
    std::filesystem::path project_root,
    std::string* error) {

    close();

    std::error_code ec;
    auto absolute = std::filesystem::absolute(project_root, ec);
    root_ = (ec ? std::move(project_root) : std::move(absolute))
        .lexically_normal();

    assets_path_ = root_ / "Assets";
    manifest_path_ = root_ / "NEngine.nproject";

    const std::filesystem::path directories[] = {
        assets_path_,
        assets_path_ / "Scenes",
        assets_path_ / "Scripts",
        assets_path_ / "Materials",
        root_ / "ProjectSettings",
        root_ / "Packages",
        root_ / "Library",
        root_ / "Library" / "Cache",
    };

    for (const auto& directory : directories) {
        ec.clear();
        std::filesystem::create_directories(directory, ec);
        if (ec) {
            if (error) {
                *error =
                    "could not create project directory: " +
                    directory.generic_string();
            }
            close();
            return false;
        }
    }

    const auto managed_packages_path =
        root_ /
        "Packages" /
        "managed-packages.txt";

    ec.clear();

    if (!std::filesystem::exists(
            managed_packages_path,
            ec) ||
        ec) {

        ec.clear();

        std::ofstream packages(
            managed_packages_path,
            std::ios::binary |
                std::ios::trunc);

        if (!packages) {
            if (error) {
                *error =
                    "could not create managed package manifest";
            }
            close();
            return false;
        }

        packages
            << "# NEngine managed NuGet package references\n"
            << "# Format: Package.Id Version\n"
            << "# Example:\n"
            << "# Newtonsoft.Json 13.0.3\n";

        packages.flush();

        if (!packages.good()) {
            if (error) {
                *error =
                    "failed writing managed package manifest";
            }
            close();
            return false;
        }
    }

    ec.clear();

    if (std::filesystem::exists(
            manifest_path_,
            ec) &&
        !ec) {

        if (!ProjectManifestSerializer::load(
                manifest_path_,
                manifest_,
                error)) {
            close();
            return false;
        }
    } else {
        ec.clear();
        manifest_ = ProjectManifest{};

        if (!ProjectManifestSerializer::save(
                manifest_,
                manifest_path_,
                error)) {
            close();
            return false;
        }
    }

    const auto startup_scene =
        startup_scene_path();

    ec.clear();

    if (!std::filesystem::exists(
            startup_scene,
            ec) ||
        ec) {

        ec.clear();

        std::filesystem::create_directories(
            startup_scene.parent_path(),
            ec);

        if (ec) {
            if (error) {
                *error =
                    "could not create startup scene directory";
            }
            close();
            return false;
        }

        core::World default_world;

        const auto camera =
            default_world.create(
                "Main Camera");

        default_world.transform(camera)
            ->local_position =
            {0.0f, 4.0f, -8.0f};

        default_world.add_component<
            render::Camera>(
                camera,
                render::camera_type());

        const auto light =
            default_world.create(
                "Directional Light");

        default_world.transform(light)
            ->local_rotation =
            {0.35f, -0.2f, 0.0f, 0.91f};

        default_world.add_component<
            render::Light>(
                light,
                render::light_type());

        const auto cube =
            default_world.create(
                "Cube");

        if (auto* renderer =
                default_world.add_component<
                    render::MeshRenderer>(
                        cube,
                        render::mesh_renderer_type())) {

            renderer->mesh =
                render::
                    builtin_unit_cube_mesh_guid();
        }

        const auto child =
            default_world.create(
                "Child Cube");

        default_world.transform(child)
            ->local_position =
            {2.0f, 0.0f, 1.0f};

        default_world.set_parent(
            child,
            cube);

        if (auto* renderer =
                default_world.add_component<
                    render::MeshRenderer>(
                        child,
                        render::mesh_renderer_type())) {

            renderer->mesh =
                render::
                    builtin_unit_cube_mesh_guid();
        }

        core::ComponentSerializationRegistry
            bootstrap_serialization;

        render::register_component_serializers(
            bootstrap_serialization);

        const auto scene =
            core::SceneSerializer::capture(
                default_world,
                startup_scene
                    .stem()
                    .string(),
                &bootstrap_serialization);

        if (!core::SceneSerializer::save_file(
                scene,
                startup_scene,
                error)) {
            close();
            return false;
        }
    }

    assets_.set_root(assets_path_);
    assets_.set_importers(&importers_);
    watcher_.set_root(assets_path_);

    assets_.scan(true);
    watcher_.poll();

    open_ = true;
    return true;
}

void ProjectSession::close() {
    open_ = false;
    root_.clear();
    assets_path_.clear();
    manifest_path_.clear();
    manifest_ = ProjectManifest{};
    assets_.clear();
    watcher_.set_root({});
    dependency_graph_.clear();
}

assets::AssetScanResult ProjectSession::refresh_assets() {
    if (!open_) return {};
    return assets_.scan(true);
}

AssetPollResult ProjectSession::poll_assets() {
    AssetPollResult result;
    if (!open_) return result;

    result.changes = watcher_.poll();
    if (result.changes.empty()) {
        return result;
    }

    std::vector<assets::AssetGuid> removed_guids;
    removed_guids.reserve(result.changes.size());

    for (const auto& change : result.changes) {
        if (change.kind !=
            assets::FileChangeKind::Removed) {
            continue;
        }

        if (const auto* record =
                assets_.find_relative(
                    change.relative_path
                        .generic_string())) {
            removed_guids.push_back(
                record->guid);
        }
    }

    result.scan = assets_.scan(true);

    // Determine dependent assets before removal clears reverse graph edges.
    // A touched texture should invalidate its .nmat users, and a touched
    // external .bin/PNG should automatically reimport its referencing glTF.
    std::unordered_set<assets::AssetGuid, assets::AssetGuidHash>
        changed_assets{removed_guids.begin(), removed_guids.end()};

    for (const auto& change : result.changes) {
        if (change.kind == assets::FileChangeKind::Removed) continue;
        if (const auto* record = assets_.find_relative(
                change.relative_path.generic_string())) {
            changed_assets.insert(record->guid);
        }
    }

    std::deque<assets::AssetGuid> pending;
    for (const auto guid : changed_assets) pending.push_back(guid);

    std::unordered_set<assets::AssetGuid, assets::AssetGuidHash>
        all_affected = changed_assets;

    while (!pending.empty()) {
        const auto guid = pending.front();
        pending.pop_front();
        for (const auto dependent : dependency_graph_.dependents(guid)) {
            if (all_affected.insert(dependent).second) {
                pending.push_back(dependent);
            }
        }
    }

    for (const auto guid : removed_guids) {
        // Keep incoming edges to a temporarily missing AssetGuid so
        // restoring the file with its persistent .meta automatically
        // reimports any model/material that referenced it. Its own
        // outgoing dependencies are no longer meaningful.
        dependency_graph_.set_dependencies(guid, {});
    }

    std::unordered_set<assets::AssetGuid, assets::AssetGuidHash>
        imported_assets;

    for (const auto& change : result.changes) {
        if (change.kind ==
            assets::FileChangeKind::Removed) {
            continue;
        }

        const auto* record =
            assets_.find_relative(
                change.relative_path
                    .generic_string());

        if (!record) {
            continue;
        }

        if (!import_pipeline_.has_processor(
                record->importer_id)) {
            ++result.imports.unsupported;
            continue;
        }

        if (!imported_assets.insert(record->guid).second) continue;
        ++result.imports.attempted;

        const auto imported =
            import_asset(record->guid);

        if (!imported.success) {
            ++result.imports.failed;
        } else if (imported.cache_hit) {
            ++result.imports.cache_hits;
        } else {
            ++result.imports.imported;
        }
    }

    // A sidecar may change without its parent .gltf changing. Reimport
    // transitive dependents exactly once, after directly changed assets.
    for (const auto guid : all_affected) {
        if (imported_assets.contains(guid)) continue;
        if (const auto* record = assets_.find(guid)) {
            if (!import_pipeline_.has_processor(record->importer_id)) {
                ++result.imports.unsupported;
                continue;
            }
            ++result.imports.attempted;
            const auto imported = import_asset(guid);
            if (!imported.success) ++result.imports.failed;
            else if (imported.cache_hit) ++result.imports.cache_hits;
            else ++result.imports.imported;
        }
    }

    return result;
}

assets::ImportResult ProjectSession::import_asset(
    assets::AssetGuid guid) {

    if (!open_) {
        assets::ImportResult result;
        result.message = "project is not open";
        return result;
    }

    const auto* record =
        assets_.find(guid);

    if (!record) {
        assets::ImportResult result;
        result.message = "asset not found";
        return result;
    }

    auto result =
        import_pipeline_.import(
            *record,
            importers_,
            root_ / "Library" / "Cache");

    if (result.success) {
        if (record->importer_id == "NEngine.Model" &&
            record->source_path.extension() == ".gltf") {

            std::vector<assets::GltfSidecar> sidecars;
            if (assets::collect_gltf_sidecars(
                    record->source_path, sidecars)) {
                for (const auto& sidecar : sidecars) {
                    const auto relative =
                        (record->relative_path.parent_path() /
                            sidecar.relative_path).lexically_normal();

                    if (const auto* dependency =
                            assets_.find_relative(relative.generic_string())) {
                        result.dependencies.push_back(dependency->guid);
                    }
                }
            }
        }

        // V1 import manifests did not persist dependency GUIDs; recover
        // a cached .nmat dependency without mutating its legacy source.
        if (result.cache_hit &&
            record->importer_id == "NEngine.Material" &&
            result.dependencies.empty()) {
            std::ifstream source(record->source_path, std::ios::binary);
            render::MaterialAssetData material;
            if (source && render::read_material_asset(source, material)) {
                result.dependencies.push_back(material.base_color_texture);
            }
        }

        dependency_graph_.set_dependencies(guid, result.dependencies);
    }

    return result;
}

AssetImportSummary ProjectSession::import_supported_assets() {
    AssetImportSummary summary;
    if (!open_) return summary;

    for (const auto& record :
         assets_.records()) {

        if (!import_pipeline_.has_processor(
                record.importer_id)) {
            ++summary.unsupported;
            continue;
        }

        ++summary.attempted;

        const auto result =
            import_asset(record.guid);

        if (!result.success) {
            ++summary.failed;
        } else if (result.cache_hit) {
            ++summary.cache_hits;
        } else {
            ++summary.imported;
        }
    }

    return summary;
}

std::optional<assets::CachedArtifactSet>
ProjectSession::cached_artifacts(
    assets::AssetGuid guid) const {

    if (!open_) {
        return std::nullopt;
    }

    const auto* record =
        assets_.find(guid);

    if (!record) {
        return std::nullopt;
    }

    return import_pipeline_.cached_artifacts(
        *record,
        importers_,
        root_ / "Library" / "Cache");
}

AssetActivation ProjectSession::activation_for(
    assets::AssetGuid guid) const {

    AssetActivation activation;
    activation.guid = guid;

    if (!open_) {
        return activation;
    }

    const auto* record =
        assets_.find(guid);

    if (!record) {
        return activation;
    }

    activation.source_path =
        record->source_path;

    if (record->importer_id ==
        "NEngine.Scene") {
        activation.kind =
            AssetActivationKind::OpenScene;
    } else if (
        record->importer_id ==
        "NEngine.Script") {
        activation.kind =
            AssetActivationKind::OpenScript;
    } else {
        activation.kind =
            AssetActivationKind::OpenExternal;
    }

    return activation;
}


} // namespace nengine::editor
