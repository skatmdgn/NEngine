#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>

#include "nengine/assets/asset_guid.hpp"
#include "nengine/assets/import_pipeline.hpp"
#include "nengine/render/decoded_mesh.hpp"
#include "nengine/render/vulkan_device.hpp"
#include "nengine/render/vulkan_instance.hpp"
#include "nengine/render/vulkan_loader.hpp"
#include "nengine/render/vulkan_mesh.hpp"

namespace nengine::render {

class VulkanMeshAssetCache {
public:
    VulkanMeshAssetCache() = default;
    ~VulkanMeshAssetCache();

    VulkanMeshAssetCache(
        const VulkanMeshAssetCache&) = delete;

    VulkanMeshAssetCache& operator=(
        const VulkanMeshAssetCache&) = delete;

    bool initialize(
        const VulkanLoader& loader,
        const VulkanInstance& instance,
        const VulkanDevice& device);

    const VulkanMeshResource* load(
        assets::AssetGuid guid,
        const assets::CachedArtifactSet& artifacts,
        std::string* error = nullptr);

    const VulkanMeshResource* upload(
        assets::AssetGuid guid,
        std::string fingerprint,
        const MeshData& mesh,
        std::string* error = nullptr);

    const VulkanMeshResource* find(
        assets::AssetGuid guid) const noexcept;

    bool erase(
        assets::AssetGuid guid) noexcept;

    // Drop imported GPU/CPU mesh entries while keeping the cache
    // attached to its current Vulkan device.
    void clear() noexcept;

    void shutdown() noexcept;

    bool ready() const noexcept {
        return loader_ &&
            instance_ &&
            device_ &&
            instance_->valid() &&
            device_->valid();
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
        VulkanMeshResource mesh{};
    };

    const VulkanLoader* loader_{nullptr};
    const VulkanInstance* instance_{nullptr};
    const VulkanDevice* device_{nullptr};

    DecodedMeshCache decoded_cache_{};

    std::unordered_map<
        assets::AssetGuid,
        Entry,
        assets::AssetGuidHash> entries_{};

    std::string diagnostic_{};
};

} // namespace nengine::render
