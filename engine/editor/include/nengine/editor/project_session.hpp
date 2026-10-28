#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "nengine/assets/asset_database.hpp"
#include "nengine/assets/asset_importer.hpp"
#include "nengine/assets/dependency_graph.hpp"
#include "nengine/assets/file_watcher.hpp"
#include "nengine/assets/import_pipeline.hpp"
#include "nengine/editor/project_manifest.hpp"
#include "nengine/navigation/nav_mesh.hpp"

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

enum class AssetActivationKind : std::uint8_t {
    None,
    OpenScene,
    OpenScript,
    OpenExternal,
};

struct AssetActivation {
    AssetActivationKind kind{
        AssetActivationKind::None};
    assets::AssetGuid guid{};
    std::filesystem::path source_path{};
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

    std::filesystem::path startup_scene_path() const {
        return root_ / manifest_.startup_scene;
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

    std::optional<assets::CachedArtifactSet>
    cached_artifacts(
        assets::AssetGuid guid) const;

    AssetActivation activation_for(
        assets::AssetGuid guid) const;

    // EDIT-013: Asset Browser operations preserve GUID/meta identity.
    // Mutations are confined under Assets and refresh the asset index.
    bool import_external_asset(const std::filesystem::path& source,
                               assets::AssetGuid* guid=nullptr,
                               std::string* error=nullptr);
    bool rename_asset(assets::AssetGuid guid,std::string new_stem,
                      std::string* error=nullptr);
    bool delete_asset(assets::AssetGuid guid,std::string* error=nullptr);

    // Resolve imported, confined project navigation data by stable AssetGuid.
    bool load_nav_mesh(assets::AssetGuid guid,
                       navigation::NavMesh& output,
                       std::string* error = nullptr) const;

private:
    void register_builtin_importers();

    void index_generated_subassets(
        assets::AssetGuid parent_guid,
        const std::vector<
            assets::GeneratedSubasset>&
            subassets);

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

    mutable std::unordered_map<
        assets::AssetGuid,
        assets::AssetGuid,
        assets::AssetGuidHash>
        generated_subasset_parents_{};
};

} // namespace nengine::editor
