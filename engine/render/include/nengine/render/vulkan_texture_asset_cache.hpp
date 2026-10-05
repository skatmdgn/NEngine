#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>

#include "nengine/assets/asset_guid.hpp"
#include "nengine/assets/import_pipeline.hpp"
#include "nengine/render/decoded_texture.hpp"
#include "nengine/render/vulkan_device.hpp"
#include "nengine/render/vulkan_instance.hpp"
#include "nengine/render/vulkan_loader.hpp"
#include "nengine/render/vulkan_material.hpp"
#include "nengine/render/vulkan_texture.hpp"

namespace nengine::render {

struct VulkanTextureAssetResource {
    VulkanTextureResource texture{};
    VulkanMaterialResource material{};

    bool valid() const noexcept {
        return texture.valid() &&
            material.valid();
    }
};

class VulkanTextureAssetCache {
public:
    VulkanTextureAssetCache() = default;
    ~VulkanTextureAssetCache();

    VulkanTextureAssetCache(
        const VulkanTextureAssetCache&) = delete;

    VulkanTextureAssetCache& operator=(
        const VulkanTextureAssetCache&) = delete;

    bool initialize(
        const VulkanLoader& loader,
        const VulkanInstance& instance,
        const VulkanDevice& device);

    const VulkanTextureAssetResource* load(
        assets::AssetGuid guid,
        const assets::CachedArtifactSet& artifacts,
        std::string* error = nullptr);

    const VulkanTextureAssetResource* upload(
        assets::AssetGuid guid,
        std::string fingerprint,
        const DecodedTextureData& decoded,
        std::string* error = nullptr);

    const VulkanTextureAssetResource* find(
        assets::AssetGuid guid) const noexcept;

    bool erase(
        assets::AssetGuid guid) noexcept;

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
        VulkanTextureAssetResource resource{};
    };

    const VulkanLoader* loader_{nullptr};
    const VulkanInstance* instance_{nullptr};
    const VulkanDevice* device_{nullptr};

    DecodedTextureCache decoded_cache_{};

    std::unordered_map<
        assets::AssetGuid,
        Entry,
        assets::AssetGuidHash> entries_{};

    std::string diagnostic_{};
};

} // namespace nengine::render
