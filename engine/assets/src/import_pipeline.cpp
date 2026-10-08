#include "nengine/assets/import_pipeline.hpp"
#include "nengine/assets/gltf_sidecars.hpp"

#include <cctype>

#include <fstream>
#include <iomanip>
#include <sstream>
#include <unordered_set>
#include <system_error>
#include <utility>

namespace nengine::assets {
namespace {

struct CachedManifest {
    std::string fingerprint{};
    std::string importer_id{};
    std::uint32_t importer_version{0};
    std::vector<ImportArtifact> artifacts{};
    std::vector<AssetGuid> dependencies{};
    std::vector<GeneratedSubasset> subassets{};
};

std::filesystem::path manifest_path(
    const std::filesystem::path& directory) {

    return directory / "import.meta";
}

bool read_manifest(
    const std::filesystem::path& path,
    CachedManifest& manifest) {

    std::ifstream input(path, std::ios::binary);
    if (!input) return false;

    std::string token;
    std::uint32_t version = 0;

    if (!(input >> token >> version) ||
        token != "NENGINE_IMPORT" ||
        (version != 1 &&
         version != 2 &&
         version != 3)) {
        return false;
    }

    if (!(input >> token) ||
        token != "FINGERPRINT" ||
        !(input >> std::quoted(manifest.fingerprint))) {
        return false;
    }

    if (!(input >> token) ||
        token != "IMPORTER" ||
        !(input >> std::quoted(manifest.importer_id))) {
        return false;
    }

    if (!(input >> token) ||
        token != "VERSION" ||
        !(input >> manifest.importer_version)) {
        return false;
    }

    std::size_t artifact_count = 0;
    if (!(input >> token) ||
        token != "ARTIFACTS" ||
        !(input >> artifact_count)) {
        return false;
    }

    manifest.artifacts.clear();
    manifest.artifacts.reserve(artifact_count);

    for (std::size_t i = 0; i < artifact_count; ++i) {
        ImportArtifact artifact;
        std::string path_text;

        if (!(input >> token) ||
            token != "ARTIFACT" ||
            !(input >> std::quoted(artifact.role)) ||
            !(input >> std::quoted(path_text))) {
            return false;
        }

        artifact.path =
            std::filesystem::path{
                std::move(path_text)
            };

        manifest.artifacts.push_back(
            std::move(artifact));
    }

    manifest.dependencies.clear();
    if (version >= 2u) {
        std::size_t dependency_count = 0;
        if (!(input >> token) ||
            token != "DEPENDENCIES" ||
            !(input >> dependency_count) ||
            dependency_count > 8192u) {
            return false;
        }

        for (std::size_t i = 0; i < dependency_count; ++i) {
            std::string guid_text;
            if (!(input >> token) ||
                token != "DEPENDENCY" ||
                !(input >> std::quoted(guid_text))) {
                return false;
            }
            const auto guid = AssetGuid::parse(guid_text);
            if (!guid || !guid->valid()) return false;
            manifest.dependencies.push_back(*guid);
        }
    }

    manifest.subassets.clear();

    if (version >= 3u) {
        std::size_t subasset_count = 0;

        if (!(input >> token) ||
            token != "SUBASSETS" ||
            !(input >> subasset_count) ||
            subasset_count > 8192u) {
            return false;
        }

        manifest.subassets.reserve(
            subasset_count);

        for (std::size_t i = 0;
             i < subasset_count;
             ++i) {

            GeneratedSubasset subasset;
            std::string guid_text;
            std::size_t artifact_count = 0;

            if (!(input >> token) ||
                token != "SUBASSET" ||
                !(input >> std::quoted(guid_text)) ||
                !(input >> std::quoted(subasset.importer_id)) ||
                !(input >> std::quoted(subasset.name)) ||
                !(input >> artifact_count) ||
                artifact_count == 0u ||
                artifact_count > 1024u) {
                return false;
            }

            const auto guid =
                AssetGuid::parse(
                    guid_text);

            if (!guid ||
                !guid->valid()) {
                return false;
            }

            subasset.guid =
                *guid;

            subasset.artifacts.reserve(
                artifact_count);

            for (std::size_t artifact_index = 0;
                 artifact_index < artifact_count;
                 ++artifact_index) {

                ImportArtifact artifact;
                std::string path_text;

                if (!(input >> token) ||
                    token != "SUBARTIFACT" ||
                    !(input >> std::quoted(artifact.role)) ||
                    !(input >> std::quoted(path_text))) {
                    return false;
                }

                artifact.path =
                    std::filesystem::path{
                        std::move(
                            path_text)};

                subasset.artifacts.push_back(
                    std::move(
                        artifact));
            }

            if (!subasset.valid()) {
                return false;
            }

            manifest.subassets.push_back(
                std::move(
                    subasset));
        }
    }

    return (input >> token) &&
        token == "END_IMPORT";
}

bool write_manifest(
    const std::filesystem::path& path,
    const CachedManifest& manifest) {

    std::ofstream output(
        path,
        std::ios::binary | std::ios::trunc);

    if (!output) return false;

    output << "NENGINE_IMPORT 3\n";
    output << "FINGERPRINT "
           << std::quoted(manifest.fingerprint)
           << "\n";
    output << "IMPORTER "
           << std::quoted(manifest.importer_id)
           << "\n";
    output << "VERSION "
           << manifest.importer_version
           << "\n";
    output << "ARTIFACTS "
           << manifest.artifacts.size()
           << "\n";

    for (const auto& artifact :
         manifest.artifacts) {
        output << "ARTIFACT "
               << std::quoted(artifact.role)
               << " "
               << std::quoted(
                    artifact.path.generic_string())
               << "\n";
    }

    output << "DEPENDENCIES "
           << manifest.dependencies.size()
           << "\n";
    for (const auto guid : manifest.dependencies) {
        output << "DEPENDENCY "
               << std::quoted(guid.to_string())
               << "\n";
    }

    output << "SUBASSETS "
           << manifest.subassets.size()
           << "\n";

    for (const auto& subasset :
         manifest.subassets) {

        output << "SUBASSET "
               << std::quoted(
                    subasset.guid.to_string())
               << " "
               << std::quoted(
                    subasset.importer_id)
               << " "
               << std::quoted(
                    subasset.name)
               << " "
               << subasset.artifacts.size()
               << "\n";

        for (const auto& artifact :
             subasset.artifacts) {

            output << "SUBARTIFACT "
                   << std::quoted(
                        artifact.role)
                   << " "
                   << std::quoted(
                        artifact.path
                            .generic_string())
                   << "\n";
        }
    }

    output << "END_IMPORT\n";
    return output.good();
}

bool artifacts_exist(
    const std::filesystem::path& cache_directory,
    const CachedManifest& manifest) {

    std::error_code error;

    for (const auto& artifact :
         manifest.artifacts) {

        auto path = artifact.path;

        if (path.is_relative()) {
            path = cache_directory / path;
        }

        if (!std::filesystem::exists(
                path,
                error) ||
            error) {
            return false;
        }
    }

    for (const auto& subasset :
         manifest.subassets) {

        if (!subasset.valid()) {
            return false;
        }

        for (const auto& artifact :
             subasset.artifacts) {

            auto path =
                artifact.path;

            if (path.is_relative()) {
                path =
                    cache_directory /
                    path;
            }

            if (!std::filesystem::exists(
                    path,
                    error) ||
                error) {
                return false;
            }
        }
    }

    return true;
}

std::optional<std::filesystem::path>
safe_cache_artifact_path(
    const std::filesystem::path& cache_directory,
    const std::filesystem::path& stored) {

    std::error_code error;

    const auto cache =
        std::filesystem::absolute(
            cache_directory,
            error)
            .lexically_normal();

    if (error) {
        return std::nullopt;
    }

    auto candidate =
        stored.is_relative()
            ? cache / stored
            : stored;

    candidate =
        std::filesystem::absolute(
            candidate,
            error)
            .lexically_normal();

    if (error) {
        return std::nullopt;
    }

    const auto relative =
        candidate.lexically_relative(
            cache);

    if (relative.empty() ||
        relative == "." ||
        relative.has_root_path()) {
        return std::nullopt;
    }

    for (const auto& part :
         relative) {
        if (part == "..") {
            return std::nullopt;
        }
    }

    return candidate;
}

std::vector<std::filesystem::path>
manifest_artifact_paths(
    const std::filesystem::path& cache_directory,
    const CachedManifest& manifest) {

    std::vector<std::filesystem::path>
        paths;

    const auto append =
        [&](const auto& artifacts) {

            for (const auto& artifact :
                 artifacts) {

                if (const auto safe =
                        safe_cache_artifact_path(
                            cache_directory,
                            artifact.path)) {
                    paths.push_back(
                        *safe);
                }
            }
        };

    append(
        manifest.artifacts);

    for (const auto& subasset :
         manifest.subassets) {
        append(
            subasset.artifacts);
    }

    std::sort(
        paths.begin(),
        paths.end());

    paths.erase(
        std::unique(
            paths.begin(),
            paths.end()),
        paths.end());

    return paths;
}

void cleanup_stale_artifacts(
    const std::filesystem::path& cache_directory,
    const CachedManifest& previous,
    const CachedManifest& current) {

    const auto old_paths =
        manifest_artifact_paths(
            cache_directory,
            previous);

    const auto new_paths =
        manifest_artifact_paths(
            cache_directory,
            current);

    std::unordered_set<std::string>
        retained;

    retained.reserve(
        new_paths.size());

    for (const auto& path :
         new_paths) {
        retained.insert(
            path.generic_string());
    }

    std::error_code error;

    for (const auto& path :
         old_paths) {

        if (retained.contains(
                path.generic_string())) {
            continue;
        }

        error.clear();

        if (std::filesystem::
                is_regular_file(
                    path,
                    error) &&
            !error) {

            std::filesystem::remove(
                path,
                error);
        }

        auto parent =
            path.parent_path();

        while (!error &&
               !parent.empty() &&
               parent !=
                   cache_directory) {

            if (!std::filesystem::
                    is_directory(
                        parent,
                        error) ||
                error ||
                !std::filesystem::
                    is_empty(
                        parent,
                        error) ||
                error) {
                break;
            }

            std::filesystem::remove(
                parent,
                error);

            parent =
                parent.parent_path();
        }
    }
}

} // namespace

bool AssetImportPipeline::register_processor(
    std::string importer_id,
    Processor processor) {

    if (importer_id.empty() ||
        !processor ||
        processors_.contains(importer_id)) {
        return false;
    }

    processors_.emplace(
        std::move(importer_id),
        std::move(processor));

    return true;
}

bool AssetImportPipeline::has_processor(
    std::string_view importer_id) const noexcept {

    return processors_.contains(
        std::string{importer_id});
}

std::string AssetImportPipeline::fingerprint(
    const AssetRecord& asset,
    const ImporterDescriptor& importer) {

    std::ostringstream stream;

    stream
        << asset.guid.to_string()
        << ":"
        << importer.id
        << ":"
        << importer.version
        << ":"
        << asset.file_size
        << ":"
        << asset.write_stamp;

    if (asset.importer_id == "NEngine.Model") {
        auto extension = asset.source_path.extension().string();
        for (char& ch : extension) {
            ch = static_cast<char>(
                std::tolower(static_cast<unsigned char>(ch)));
        }
        if (extension == ".gltf") {
            stream << gltf_sidecar_fingerprint(asset.source_path);
        }
    }

    return stream.str();
}

std::optional<CachedArtifactSet>
AssetImportPipeline::cached_artifacts(
    const AssetRecord& asset,
    const ImporterRegistry& registry,
    const std::filesystem::path& cache_root) const {

    const auto* importer =
        registry.find(asset.importer_id);

    if (!importer) {
        return std::nullopt;
    }

    const auto cache_directory =
        cache_root /
        asset.guid.to_string();

    CachedManifest cached;

    if (!read_manifest(
            manifest_path(cache_directory),
            cached)) {
        return std::nullopt;
    }

    if (cached.fingerprint !=
            fingerprint(asset, *importer) ||
        cached.importer_id !=
            importer->id ||
        cached.importer_version !=
            importer->version ||
        !artifacts_exist(
            cache_directory,
            cached)) {
        return std::nullopt;
    }

    CachedArtifactSet result;
    result.fingerprint =
        cached.fingerprint;
    result.importer_id =
        cached.importer_id;
    result.importer_version =
        cached.importer_version;

    result.artifacts.reserve(
        cached.artifacts.size());

    for (auto artifact :
         cached.artifacts) {

        if (artifact.path.is_relative()) {
            artifact.path =
                cache_directory /
                artifact.path;
        }

        result.artifacts.push_back(
            std::move(artifact));
    }

    return result;
}

std::optional<CachedArtifactSet>
AssetImportPipeline::cached_subasset_artifacts(
    const AssetRecord& parent_asset,
    AssetGuid subasset_guid,
    const ImporterRegistry& registry,
    const std::filesystem::path& cache_root) const {

    if (!subasset_guid.valid()) {
        return std::nullopt;
    }

    const auto* importer =
        registry.find(
            parent_asset.importer_id);

    if (!importer) {
        return std::nullopt;
    }

    const auto cache_directory =
        cache_root /
        parent_asset.guid.to_string();

    CachedManifest cached;

    if (!read_manifest(
            manifest_path(
                cache_directory),
            cached) ||
        cached.fingerprint !=
            fingerprint(
                parent_asset,
                *importer) ||
        cached.importer_id !=
            importer->id ||
        cached.importer_version !=
            importer->version ||
        !artifacts_exist(
            cache_directory,
            cached)) {

        return std::nullopt;
    }

    for (auto subasset :
         cached.subassets) {

        if (subasset.guid !=
            subasset_guid) {
            continue;
        }

        CachedArtifactSet result;
        result.fingerprint =
            cached.fingerprint +
            ":subasset:" +
            subasset.guid.to_string();
        result.importer_id =
            subasset.importer_id;
        result.importer_version = 1u;

        result.artifacts.reserve(
            subasset.artifacts.size());

        for (auto artifact :
             subasset.artifacts) {

            if (artifact.path.is_relative()) {
                artifact.path =
                    cache_directory /
                    artifact.path;
            }

            result.artifacts.push_back(
                std::move(
                    artifact));
        }

        return result;
    }

    return std::nullopt;
}

ImportResult AssetImportPipeline::import(
    const AssetRecord& asset,
    const ImporterRegistry& registry,
    const std::filesystem::path& cache_root) const {

    ImportResult result;

    const auto* importer =
        registry.find(asset.importer_id);

    if (!importer) {
        result.message =
            "importer descriptor not found: " +
            asset.importer_id;
        return result;
    }

    const auto processor_it =
        processors_.find(importer->id);

    if (processor_it == processors_.end()) {
        result.message =
            "importer processor not implemented: " +
            importer->id;
        return result;
    }

    const auto cache_directory =
        cache_root / asset.guid.to_string();

    std::error_code error;
    std::filesystem::create_directories(
        cache_directory,
        error);

    if (error) {
        result.message =
            "could not create import cache directory";
        return result;
    }

    const auto expected_fingerprint =
        fingerprint(asset, *importer);

    CachedManifest cached;

    const bool had_cached_manifest =
        read_manifest(
            manifest_path(
                cache_directory),
            cached);

    if (had_cached_manifest &&
        cached.fingerprint ==
            expected_fingerprint &&
        cached.importer_id ==
            importer->id &&
        cached.importer_version ==
            importer->version &&
        artifacts_exist(
            cache_directory,
            cached)) {

        result.success = true;
        result.cache_hit = true;
        result.message = "cache hit";
        result.dependencies = cached.dependencies;

        for (auto artifact :
             cached.artifacts) {

            if (artifact.path.is_relative()) {
                artifact.path =
                    cache_directory /
                    artifact.path;
            }

            result.artifacts.push_back(
                std::move(artifact));
        }

        result.subassets.reserve(
            cached.subassets.size());

        for (auto subasset :
             cached.subassets) {

            for (auto& artifact :
                 subasset.artifacts) {

                if (artifact.path.is_relative()) {
                    artifact.path =
                        cache_directory /
                        artifact.path;
                }
            }

            result.subassets.push_back(
                std::move(
                    subasset));
        }

        return result;
    }

    const ImportContext context{
        &asset,
        importer,
        cache_directory
    };

    result =
        processor_it->second(context);

    if (!result.success) {
        return result;
    }

    CachedManifest manifest;
    manifest.fingerprint =
        expected_fingerprint;
    manifest.importer_id =
        importer->id;
    manifest.importer_version =
        importer->version;
    manifest.dependencies = result.dependencies;

    for (const auto& subasset :
         result.subassets) {

        if (!subasset.valid()) {
            result.success = false;
            result.message =
                "import produced invalid generated subasset";
            return result;
        }

        GeneratedSubasset stored =
            subasset;

        for (auto& artifact :
             stored.artifacts) {

            std::error_code
                relative_error;

            const auto relative =
                std::filesystem::relative(
                    artifact.path,
                    cache_directory,
                    relative_error);

            if (!relative_error &&
                !relative.empty()) {
                artifact.path =
                    relative;
            }
        }

        manifest.subassets.push_back(
            std::move(
                stored));
    }

    for (const auto& artifact :
         result.artifacts) {

        ImportArtifact stored =
            artifact;

        std::error_code relative_error;

        const auto relative =
            std::filesystem::relative(
                artifact.path,
                cache_directory,
                relative_error);

        if (!relative_error &&
            !relative.empty()) {
            stored.path = relative;
        }

        manifest.artifacts.push_back(
            std::move(stored));
    }

    if (!write_manifest(
            manifest_path(cache_directory),
            manifest)) {

        result.success = false;
        result.message =
            "import succeeded but cache manifest write failed";
        return result;
    }

    if (had_cached_manifest) {
        cleanup_stale_artifacts(
            cache_directory,
            cached,
            manifest);
    }

    return result;
}

ImportResult copy_source_importer(
    const ImportContext& context) {

    ImportResult result;

    if (!context.asset ||
        !context.importer) {
        result.message =
            "invalid import context";
        return result;
    }

    const auto extension =
        context.asset->source_path.extension();

    const auto destination =
        context.cache_directory /
        (std::string{"source"} +
         extension.string());

    std::error_code error;

    std::filesystem::copy_file(
        context.asset->source_path,
        destination,
        std::filesystem::copy_options::overwrite_existing,
        error);

    if (error) {
        result.message =
            "source copy failed";
        return result;
    }

    result.success = true;
    result.message = "source staged";
    result.artifacts.push_back({
        destination,
        "source"
    });

    return result;
}

} // namespace nengine::assets
