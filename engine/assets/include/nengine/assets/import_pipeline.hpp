#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "nengine/assets/asset_database.hpp"
#include "nengine/assets/asset_guid.hpp"
#include "nengine/assets/asset_importer.hpp"

namespace nengine::assets {

struct ImportArtifact {
    std::filesystem::path path{};
    std::string role{};
};

struct ImportContext {
    const AssetRecord* asset{nullptr};
    const ImporterDescriptor* importer{nullptr};
    std::filesystem::path cache_directory{};
};

struct ImportResult {
    bool success{false};
    bool cache_hit{false};
    std::string message{};
    std::vector<ImportArtifact> artifacts{};
    std::vector<AssetGuid> dependencies{};
};

struct CachedArtifactSet {
    std::string fingerprint{};
    std::string importer_id{};
    std::uint32_t importer_version{0};
    std::vector<ImportArtifact> artifacts{};
};

class AssetImportPipeline {
public:
    using Processor =
        std::function<ImportResult(const ImportContext&)>;

    bool register_processor(
        std::string importer_id,
        Processor processor);

    bool has_processor(
        std::string_view importer_id) const noexcept;

    ImportResult import(
        const AssetRecord& asset,
        const ImporterRegistry& registry,
        const std::filesystem::path& cache_root) const;

    std::optional<CachedArtifactSet> cached_artifacts(
        const AssetRecord& asset,
        const ImporterRegistry& registry,
        const std::filesystem::path& cache_root) const;

    static std::string fingerprint(
        const AssetRecord& asset,
        const ImporterDescriptor& importer);

private:
    std::unordered_map<std::string, Processor> processors_{};
};

ImportResult copy_source_importer(
    const ImportContext& context);

} // namespace nengine::assets
