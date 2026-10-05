#pragma once

#include <string>

#include "nengine/render/vulkan_device.hpp"
#include "nengine/render/vulkan_texture.hpp"

namespace nengine::render {

class VulkanMaterialResource {
public:
    VulkanMaterialResource() = default;
    ~VulkanMaterialResource();

    VulkanMaterialResource(
        const VulkanMaterialResource&) = delete;

    VulkanMaterialResource& operator=(
        const VulkanMaterialResource&) = delete;

    VulkanMaterialResource(
        VulkanMaterialResource&& other) noexcept;

    VulkanMaterialResource& operator=(
        VulkanMaterialResource&& other) noexcept;

    bool create_textured(
        const VulkanDevice& device,
        const VulkanTextureResource& texture);

    void destroy() noexcept;

    bool valid() const noexcept {
        return descriptor_set_layout_ != nullptr &&
            descriptor_pool_ != nullptr &&
            descriptor_set_ != nullptr;
    }

    void* native_descriptor_set_layout() const noexcept {
        return descriptor_set_layout_;
    }

    void* native_descriptor_set() const noexcept {
        return descriptor_set_;
    }

    const std::string& diagnostic() const noexcept {
        return diagnostic_;
    }

private:
    const VulkanDevice* device_api_{nullptr};
    void* device_{nullptr};
    void* descriptor_set_layout_{nullptr};
    void* descriptor_pool_{nullptr};
    void* descriptor_set_{nullptr};
    std::string diagnostic_{};
};

} // namespace nengine::render
