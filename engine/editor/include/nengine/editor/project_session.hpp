#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "nengine/assets/asset_database.hpp"
#include "nengine/assets/asset_importer.hpp"
#include "nengine/assets/dependency_graph.hpp"
#include "nengine/assets/file_watcher.hpp"

namespace nengine::editor {

struct AssetPollResult {
    std::vector<assets::FileChange> changes{};
    assets::AssetScanResult scan{};
};

class ProjectSession {
public:
    ProjectSession();

    bool open(
        std::filesystem::path project_root,
        std::string* error = nullptr);

    void close();
    bool is_open() const noexcept { return open_; }

    const std::filesystem::path& root() const noexcept {
        return root_;
    }

    const std::filesystem::path& assets_path() const noexcept {
        return assets_path_;
    }

    assets::AssetDatabase& assets() noexcept { return assets_; }
    const assets::AssetDatabase& assets() const noexcept { return assets_; }

    assets::ImporterRegistry& importers() noexcept { return importers_; }
    const assets::ImporterRegistry& importers() const noexcept { return importers_; }

    assets::AssetDependencyGraph& dependency_graph() noexcept {
        return dependency_graph_;
    }

    const assets::AssetDependencyGraph& dependency_graph() const noexcept {
        return dependency_graph_;
    }

    assets::AssetScanResult refresh_assets();
    AssetPollResult poll_assets();

private:
    void register_builtin_importers();

    bool open_{false};
    std::filesystem::path root_{};
    std::filesystem::path assets_path_{};

    assets::ImporterRegistry importers_{};
    assets::AssetDatabase assets_{};
    assets::PollingFileWatcher watcher_{};
    assets::AssetDependencyGraph dependency_graph_{};
};

} // namespace nengine::editor
