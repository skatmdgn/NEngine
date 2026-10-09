#include "nengine/render/vulkan_material_asset_cache.hpp"

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

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

std::uint64_t mix64(
    std::uint64_t value) noexcept {

    value +=
        0x9e3779b97f4a7c15ull;
    value =
        (value ^
         (value >> 30u)) *
        0xbf58476d1ce4e5b9ull;
    value =
        (value ^
         (value >> 27u)) *
        0x94d049bb133111ebull;
    return
        value ^
        (value >> 31u);
}

assets::AssetGuid
neutral_material_texture_guid(
    std::uint32_t binding) noexcept {

    return {
        0x4e454e47494e4550ull,
        0x42524e4555540000ull +
            static_cast<std::uint64_t>(
                binding) +
            1ull
    };
}

DecodedTextureData
neutral_material_texture(
    std::uint32_t binding) {

    DecodedTextureData decoded;
    decoded.width = 1u;
    decoded.height = 1u;

    switch (binding) {
    case 1u:
        decoded.color_space =
            DecodedTextureColorSpace::Linear;
        decoded.rgba8 = {
            128u, 128u, 255u, 255u
        };
        break;

    case 2u:
        decoded.color_space =
            DecodedTextureColorSpace::Linear;
        // glTF metallic-roughness convention: G=roughness, B=metallic.
        decoded.rgba8 = {
            255u, 255u, 0u, 255u
        };
        break;

    case 3u:
        decoded.color_space =
            DecodedTextureColorSpace::SRgb;
        decoded.rgba8 = {
            0u, 0u, 0u, 255u
        };
        break;

    case 4u:
        decoded.color_space =
            DecodedTextureColorSpace::Linear;
        decoded.rgba8 = {
            255u, 255u, 255u, 255u
        };
        break;

    default:
        decoded.color_space =
            DecodedTextureColorSpace::SRgb;
        decoded.rgba8 = {
            255u, 255u, 255u, 255u
        };
        break;
    }

    return decoded;
}

assets::AssetGuid
gltf_material_cache_guid(
    assets::AssetGuid mesh_guid,
    std::uint32_t material_slot) noexcept {

    const auto slot =
        static_cast<std::uint64_t>(
            material_slot) +
        1ull;

    assets::AssetGuid key{
        mix64(
            mesh_guid.high ^
            (slot *
             0xd6e8feb86659fd93ull)),
        mix64(
            mesh_guid.low ^
            (slot *
             0xa0761d6478bd642full))
    };

    if (!key.valid()) {
        key.low = 1u;
    }

    return key;
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

    device_ = &device;

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

    if (!ready() ||
        !device_) {

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
            material_artifacts.fingerprint &&
        existing->second.material.valid()) {

        return
            &existing->second.material;
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

    std::array<
        assets::AssetGuid,
        5u>
        texture_guids{
            resolved->material
                .base_color_texture,
            resolved->material
                .normal_texture,
            resolved->material
                .metallic_roughness_texture,
            resolved->material
                .emissive_texture,
            resolved->material
                .occlusion_texture
        };

    std::array<
        const VulkanTextureResource*,
        5u>
        textures{};

    for (std::size_t binding = 0u;
         binding < textures.size();
         ++binding) {

        auto guid =
            texture_guids[binding];

        const VulkanTextureAssetResource*
            texture = nullptr;

        if (guid.valid()) {
            const auto artifacts =
                dependency_resolver(
                    guid);

            if (!artifacts) {
                diagnostic_ =
                    "material texture cache artifacts are unavailable for binding " +
                    std::to_string(
                        binding);
                set_error(
                    error,
                    diagnostic_);
                return nullptr;
            }

            texture =
                texture_cache_.load(
                    guid,
                    *artifacts,
                    error);
        } else {
            guid =
                neutral_material_texture_guid(
                    static_cast<std::uint32_t>(
                        binding));

            const auto decoded =
                neutral_material_texture(
                    static_cast<std::uint32_t>(
                        binding));

            texture =
                texture_cache_.upload(
                    guid,
                    "builtin-neutral-pbr:" +
                        std::to_string(
                            binding),
                    decoded,
                    error);

            texture_guids[binding] =
                guid;
        }

        if (!texture ||
            !texture->valid()) {

            diagnostic_ =
                error && !error->empty()
                    ? *error
                    : "material texture upload failed";
            return nullptr;
        }

        textures[binding] =
            &texture->texture;
    }

    Entry entry;
    entry.fingerprint =
        material_artifacts.fingerprint;
    entry.texture_guids =
        texture_guids;
    entry.material_data =
        resolved->material;

    if (!entry.material.create_textured_set(
            *device_,
            textures)) {

        diagnostic_ =
            "material descriptor creation failed: " +
            entry.material.diagnostic();

        set_error(
            error,
            diagnostic_);
        return nullptr;
    }

    entries_.insert_or_assign(
        material_guid,
        std::move(entry));

    diagnostic_ =
        "Vulkan Material v2 AssetGuid resolved to fixed five-slot sampled descriptor set";

    const auto inserted =
        entries_.find(
            material_guid);

    return
        inserted != entries_.end() &&
        inserted->second.material.valid()
        ? &inserted->second.material
        : nullptr;
}

const VulkanMaterialResource*
VulkanMaterialAssetCache::find(
    assets::AssetGuid material_guid) const noexcept {

    const auto it =
        entries_.find(
            material_guid);

    return
        it != entries_.end() &&
        it->second.material.valid()
        ? &it->second.material
        : nullptr;
}

const MaterialAssetData*
VulkanMaterialAssetCache::find_material_data(
    assets::AssetGuid material_guid) const noexcept {

    const auto it =
        entries_.find(
            material_guid);

    return it !=
            entries_.end() &&
        it->second.material.valid()
        ? &it->second.material_data
        : nullptr;
}

const VulkanMaterialResource*
VulkanMaterialAssetCache::load_texture(
    assets::AssetGuid texture_guid,
    const assets::CachedArtifactSet&
        texture_artifacts,
    std::string* error) {

    if (!ready() ||
        !texture_guid.valid()) {

        set_error(
            error,
            "direct texture material cache is not ready or AssetGuid is invalid");
        return nullptr;
    }

    const auto* texture =
        texture_cache_.load(
            texture_guid,
            texture_artifacts,
            error);

    if (!texture ||
        !texture->valid()) {
        return nullptr;
    }

    diagnostic_ =
        "Texture AssetGuid resolved directly to sampled Vulkan material";

    return &texture->material;
}

const VulkanMaterialResource*
VulkanMaterialAssetCache::find_texture(
    assets::AssetGuid texture_guid) const noexcept {

    const auto* texture =
        texture_cache_.find(
            texture_guid);

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

const VulkanMaterialResource*
VulkanMaterialAssetCache::find_gltf_material(
    assets::AssetGuid mesh_guid,
    std::uint32_t material_slot) const noexcept {

    if (!mesh_guid.valid()) {
        return nullptr;
    }

    const auto key =
        gltf_material_cache_guid(
            mesh_guid,
            material_slot);

    const auto* texture =
        texture_cache_.find(
            key);

    return
        texture &&
        texture->valid()
        ? &texture->material
        : nullptr;
}

const VulkanMaterialResource*
VulkanMaterialAssetCache::load_gltf_material(
    assets::AssetGuid mesh_guid,
    std::uint32_t material_slot,
    const assets::CachedArtifactSet&
        mesh_artifacts,
    std::string* error) {

    if (!ready() ||
        !mesh_guid.valid()) {

        set_error(
            error,
            "glTF material cache is not ready or mesh AssetGuid is invalid");
        return nullptr;
    }

    const auto key =
        gltf_material_cache_guid(
            mesh_guid,
            material_slot);

    if (const auto* loaded =
            texture_cache_.find(
                key);
        loaded &&
        loaded->valid()) {

        return
            &loaded->material;
    }

    const auto missing =
        gltf_without_base_color_
            .find(
                key);

    if (missing !=
            gltf_without_base_color_
                .end() &&
        missing->second ==
            mesh_artifacts.fingerprint) {

        return nullptr;
    }

    const auto model =
        resolve_model_asset(
            mesh_guid,
            mesh_artifacts,
            error);

    if (!model) {
        return nullptr;
    }

    DecodedTextureData decoded;
    std::string decode_error;

    if (!decode_gltf_material_base_color_texture(
            *model,
            material_slot,
            decoded,
            &decode_error)) {

        gltf_without_base_color_
            .insert_or_assign(
                key,
                mesh_artifacts
                    .fingerprint);

        set_error(
            error,
            std::move(
                decode_error));

        return nullptr;
    }

    const auto* uploaded =
        texture_cache_.upload(
            key,
            mesh_artifacts.fingerprint +
                ":gltf-material:" +
                std::to_string(
                    material_slot),
            decoded,
            error);

    if (!uploaded ||
        !uploaded->valid()) {

        return nullptr;
    }

    gltf_without_base_color_.erase(
        key);

    diagnostic_ =
        "glTF primitive material uploaded as automatic Vulkan sampled material";

    return
        &uploaded->material;
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
    device_ = nullptr;
}

} // namespace nengine::render
