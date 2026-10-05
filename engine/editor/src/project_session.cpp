#include "nengine/editor/project_session.hpp"

#include <fstream>
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
    if (!result.changes.empty()) {
        result.scan = assets_.scan(true);
    }

    return result;
}

assets::ImportResult ProjectSession::import_asset(
    assets::AssetGuid guid) const {

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

    return import_pipeline_.import(
        *record,
        importers_,
        root_ / "Library" / "Cache");
}

AssetImportSummary ProjectSession::import_supported_assets() const {
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

} // namespace nengine::editor
