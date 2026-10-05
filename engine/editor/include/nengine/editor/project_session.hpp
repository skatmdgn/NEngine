#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "nengine/assets/asset_database.hpp"
#include "nengine/assets/asset_importer.hpp"
#include "nengine/assets/dependency_graph.hpp"
#include "nengine/assets/file_watcher.hpp"
#include "nengine/assets/import_pipeline.hpp"
#include "nengine/editor/project_manifest.hpp"

namespace nengine::editor {

struct AssetImportSummary {
    std::size_t attempted{0};
    std::size_t imported{0};
    std::size_t cache_hits{0};
    std::size_t unsupported{0};
    std::size_t failed{0};

    bool changed_cache() const noexcept {
        return imported != 0;
    }
};

struct AssetPollResult {
    std::vector<assets::FileChange> changes{};
    assets::AssetScanResult scan{};
    AssetImportSummary imports{};
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

    const std::filesystem::path& manifest_path() const noexcept {
        return manifest_path_;
    }

    ProjectManifest& manifest() noexcept { return manifest_; }
    const ProjectManifest& manifest() const noexcept { return manifest_; }

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

    assets::ImportResult import_asset(
        assets::AssetGuid guid);

    AssetImportSummary import_supported_assets();

private:
    void register_builtin_importers();

    bool open_{false};
    std::filesystem::path root_{};
    std::filesystem::path assets_path_{};
    std::filesystem::path manifest_path_{};
    ProjectManifest manifest_{};

    assets::ImporterRegistry importers_{};
    assets::AssetDatabase assets_{};
    assets::PollingFileWatcher watcher_{};
    assets::AssetDependencyGraph dependency_graph_{};
    assets::AssetImportPipeline import_pipeline_{};
};

} // namespace nengine::editor
