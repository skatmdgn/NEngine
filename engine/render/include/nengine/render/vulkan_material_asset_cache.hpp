#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>

#include "nengine/assets/asset_guid.hpp"
#include "nengine/assets/import_pipeline.hpp"
#include "nengine/render/vulkan_device.hpp"
#include "nengine/render/vulkan_instance.hpp"
#include "nengine/render/vulkan_loader.hpp"
#include "nengine/render/vulkan_material.hpp"
#include "nengine/render/vulkan_texture_asset_cache.hpp"

namespace nengine::render {

using MaterialDependencyResolver =
    std::function<
        std::optional<assets::CachedArtifactSet>(
            assets::AssetGuid)>;

class VulkanMaterialAssetCache {
public:
    VulkanMaterialAssetCache() = default;
    ~VulkanMaterialAssetCache();

    VulkanMaterialAssetCache(
        const VulkanMaterialAssetCache&) = delete;

    VulkanMaterialAssetCache& operator=(
        const VulkanMaterialAssetCache&) = delete;

    bool initialize(
        const VulkanLoader& loader,
        const VulkanInstance& instance,
        const VulkanDevice& device);

    const VulkanMaterialResource* load(
        assets::AssetGuid material_guid,
        const assets::CachedArtifactSet&
            material_artifacts,
        const MaterialDependencyResolver&
            dependency_resolver,
        std::string* error = nullptr);

    const VulkanMaterialResource* find(
        assets::AssetGuid material_guid) const noexcept;

    // Direct sampled-texture material path for Sprite/UI style renderers that
    // own a Texture AssetGuid rather than a separate .nmat.
    const VulkanMaterialResource* load_texture(
        assets::AssetGuid texture_guid,
        const assets::CachedArtifactSet& texture_artifacts,
        std::string* error = nullptr);

    const VulkanMaterialResource* find_texture(
        assets::AssetGuid texture_guid) const noexcept;

    // Compatibility fallback for glTF/GLB assets that predate or cannot use
    // import-time cooked material subassets. Uses the first primitive's PBR
    // base-color input.
    const VulkanMaterialResource* load_gltf_base_color(
        assets::AssetGuid mesh_guid,
        const assets::CachedArtifactSet& mesh_artifacts,
        std::string* error = nullptr);

    const VulkanMaterialResource* find_gltf_base_color(
        assets::AssetGuid mesh_guid) const noexcept;

    // Automatic per-primitive glTF material path. material_slot is the
    // original glTF materials[] index preserved by MeshSubmesh.
    const VulkanMaterialResource* load_gltf_material(
        assets::AssetGuid mesh_guid,
        std::uint32_t material_slot,
        const assets::CachedArtifactSet& mesh_artifacts,
        std::string* error = nullptr);

    const VulkanMaterialResource* find_gltf_material(
        assets::AssetGuid mesh_guid,
        std::uint32_t material_slot) const noexcept;

    bool erase(
        assets::AssetGuid material_guid) noexcept;

    void clear() noexcept;
    void shutdown() noexcept;

    bool ready() const noexcept {
        return texture_cache_.ready();
    }

    std::size_t size() const noexcept {
        return entries_.size();
    }

    const std::string& diagnostic() const noexcept {
        return diagnostic_;
    }

private:
    struct Entry {
        std::string fingerprint{};
        std::array<
            assets::AssetGuid,
            5u> texture_guids{};
        VulkanMaterialResource material{};
    };

    const VulkanDevice* device_{nullptr};
    VulkanTextureAssetCache texture_cache_{};

    std::unordered_map<
        assets::AssetGuid,
        Entry,
        assets::AssetGuidHash> entries_{};

    // A model without an embedded base-color map is normal. Avoid
    // reparsing that model every frame until its import fingerprint changes.
    std::unordered_map<
        assets::AssetGuid,
        std::string,
        assets::AssetGuidHash> gltf_without_base_color_{};

    std::string diagnostic_{};
};

} // namespace nengine::render
