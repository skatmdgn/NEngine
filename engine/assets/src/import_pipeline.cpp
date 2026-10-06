#include "nengine/assets/import_pipeline.hpp"
#include "nengine/assets/gltf_sidecars.hpp"

#include <cctype>

#include <fstream>
#include <iomanip>
#include <sstream>
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
        (version != 1 && version != 2)) {
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

    output << "NENGINE_IMPORT 2\n";
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

    return true;
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

    if (read_manifest(
            manifest_path(cache_directory),
            cached) &&
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
