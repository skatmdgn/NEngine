#include "nengine/editor/project_session.hpp"

#include <system_error>
#include <utility>

namespace nengine::editor {

ProjectSession::ProjectSession() {
    register_builtin_importers();
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

    const std::filesystem::path directories[] = {
        assets_path_,
        assets_path_ / "Scenes",
        assets_path_ / "Scripts",
        root_ / "ProjectSettings",
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

} // namespace nengine::editor
