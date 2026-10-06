#include "nengine/render/vulkan_material_asset_cache.hpp"

#include <utility>

#include "nengine/render/gltf_mesh.hpp"
#include "nengine/render/material_asset.hpp"

namespace nengine::render {
namespace {

void set_error(
    std::string* error,
    std::string message) {

    if (error) {
        *error = std::move(message);
    }
}

} // namespace

VulkanMaterialAssetCache::~VulkanMaterialAssetCache() {
    shutdown();
}

bool VulkanMaterialAssetCache::initialize(
    const VulkanLoader& loader,
    const VulkanInstance& instance,
    const VulkanDevice& device) {

    shutdown();
    diagnostic_.clear();

    if (!texture_cache_.initialize(
            loader,
            instance,
            device)) {

        diagnostic_ =
            "material texture cache initialization failed: " +
            texture_cache_.diagnostic();

        return false;
    }

    diagnostic_ =
        "Vulkan material asset cache ready";

    return true;
}

const VulkanMaterialResource*
VulkanMaterialAssetCache::load(
    assets::AssetGuid material_guid,
    const assets::CachedArtifactSet&
        material_artifacts,
    const MaterialDependencyResolver&
        dependency_resolver,
    std::string* error) {

    if (!ready()) {
        diagnostic_ =
            "Vulkan material asset cache is not initialized";
        set_error(
            error,
            diagnostic_);
        return nullptr;
    }

    if (!material_guid.valid()) {
        diagnostic_ =
            "material cache requires a valid AssetGuid";
        set_error(
            error,
            diagnostic_);
        return nullptr;
    }

    const auto existing =
        entries_.find(
            material_guid);

    if (existing !=
            entries_.end() &&
        existing->second.fingerprint ==
            material_artifacts.fingerprint) {

        const auto* texture =
            texture_cache_.find(
                existing->second
                    .texture_guid);

        if (texture &&
            texture->valid()) {
            return &texture->material;
        }
    }

    if (!dependency_resolver) {
        diagnostic_ =
            "material dependency resolver is unavailable";
        set_error(
            error,
            diagnostic_);
        return nullptr;
    }

    const auto resolved =
        resolve_material_asset(
            material_guid,
            material_artifacts,
            error);

    if (!resolved) {
        diagnostic_ =
            error && !error->empty()
                ? *error
                : "material asset resolution failed";
        return nullptr;
    }

    const auto texture_guid =
        resolved->material
            .base_color_texture;

    const auto texture_artifacts =
        dependency_resolver(
            texture_guid);

    if (!texture_artifacts) {
        diagnostic_ =
            "material base-color texture cache artifacts are unavailable";
        set_error(
            error,
            diagnostic_);
        return nullptr;
    }

    const auto* texture =
        texture_cache_.load(
            texture_guid,
            *texture_artifacts,
            error);

    if (!texture ||
        !texture->valid()) {

        diagnostic_ =
            error && !error->empty()
                ? *error
                : "material base-color texture upload failed";
        return nullptr;
    }

    Entry entry;
    entry.fingerprint =
        material_artifacts.fingerprint;
    entry.texture_guid =
        texture_guid;

    entries_.insert_or_assign(
        material_guid,
        std::move(entry));

    diagnostic_ =
        "Vulkan material AssetGuid resolved to sampled texture";

    return &texture->material;
}

const VulkanMaterialResource*
VulkanMaterialAssetCache::find(
    assets::AssetGuid material_guid) const noexcept {

    const auto it =
        entries_.find(
            material_guid);

    if (it == entries_.end()) {
        return nullptr;
    }

    const auto* texture =
        texture_cache_.find(
            it->second.texture_guid);

    return
        texture &&
        texture->valid()
        ? &texture->material
        : nullptr;
}

const VulkanMaterialResource*
VulkanMaterialAssetCache::find_gltf_base_color(
    assets::AssetGuid mesh_guid) const noexcept {

    const auto* texture =
        texture_cache_.find(mesh_guid);

    return texture && texture->valid()
        ? &texture->material
        : nullptr;
}

const VulkanMaterialResource*
VulkanMaterialAssetCache::load_gltf_base_color(
    assets::AssetGuid mesh_guid,
    const assets::CachedArtifactSet& mesh_artifacts,
    std::string* error) {

    if (!ready() || !mesh_guid.valid()) {
        set_error(error, "glTF material cache is not ready or AssetGuid is invalid");
        return nullptr;
    }

    const auto* already_loaded =
        texture_cache_.find(mesh_guid);

    if (already_loaded && already_loaded->valid()) {
        return &already_loaded->material;
    }

    const auto missing =
        gltf_without_base_color_.find(mesh_guid);

    if (missing != gltf_without_base_color_.end() &&
        missing->second == mesh_artifacts.fingerprint) {
        return nullptr;
    }

    const auto model = resolve_model_asset(
        mesh_guid,
        mesh_artifacts,
        error);

    if (!model) {
        return nullptr;
    }

    DecodedTextureData decoded;
    std::string decode_error;

    if (!decode_gltf_base_color_texture(
            *model,
            decoded,
            &decode_error)) {
        // No material or no base-color image is a valid geometry-only
        // model. Cache negative lookups to avoid I/O every rendered frame.
        gltf_without_base_color_.insert_or_assign(
            mesh_guid,
            mesh_artifacts.fingerprint);
        set_error(error, std::move(decode_error));
        return nullptr;
    }

    const auto* uploaded = texture_cache_.upload(
        mesh_guid,
        mesh_artifacts.fingerprint + ":gltf-base-color",
        decoded,
        error);

    if (!uploaded || !uploaded->valid()) {
        return nullptr;
    }

    gltf_without_base_color_.erase(mesh_guid);
    diagnostic_ =
        "glTF PBR base-color image uploaded as automatic preview material";
    return &uploaded->material;
}

bool VulkanMaterialAssetCache::erase(
    assets::AssetGuid material_guid) noexcept {

    return
        entries_.erase(
            material_guid) != 0u;
}

void VulkanMaterialAssetCache::clear() noexcept {
    entries_.clear();
    gltf_without_base_color_.clear();
    texture_cache_.clear();
}

void VulkanMaterialAssetCache::shutdown() noexcept {
    entries_.clear();
    gltf_without_base_color_.clear();
    texture_cache_.shutdown();
}

} // namespace nengine::render
