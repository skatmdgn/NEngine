#pragma once

#include <string>

#include "nengine/assets/asset_guid.hpp"
#include "nengine/render/vulkan_loader.hpp"
#include "nengine/render/vulkan_instance.hpp"
#include "nengine/render/vulkan_device.hpp"
#include "nengine/render/vulkan_mesh.hpp"

namespace nengine::render {

class VulkanBuiltinMeshCache {
public:
    VulkanBuiltinMeshCache() = default;
    ~VulkanBuiltinMeshCache();

    VulkanBuiltinMeshCache(
        const VulkanBuiltinMeshCache&) = delete;

    VulkanBuiltinMeshCache& operator=(
        const VulkanBuiltinMeshCache&) = delete;

    bool initialize(
        const VulkanLoader& loader,
        const VulkanInstance& instance,
        const VulkanDevice& device);

    void shutdown() noexcept;

    bool ready() const noexcept {
        return cube_.valid() &&
            quad_.valid();
    }

    const VulkanMeshResource* find(
        assets::AssetGuid guid) const noexcept;

    const std::string& diagnostic() const noexcept {
        return diagnostic_;
    }

private:
    VulkanMeshResource cube_{};
    VulkanMeshResource quad_{};
    std::string diagnostic_{};
};

} // namespace nengine::render
