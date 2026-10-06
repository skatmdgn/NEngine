#pragma once

#include <cstddef>
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
        assets::AssetGuid texture_guid{};
    };

    VulkanTextureAssetCache texture_cache_{};

    std::unordered_map<
        assets::AssetGuid,
        Entry,
        assets::AssetGuidHash> entries_{};

    std::string diagnostic_{};
};

} // namespace nengine::render
