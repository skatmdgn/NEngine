#pragma once

#include <cstdint>
#include <string>

#include "nengine/render/vulkan_device.hpp"

namespace nengine::render {

class VulkanRenderPass {
public:
    VulkanRenderPass() = default;
    ~VulkanRenderPass();

    VulkanRenderPass(
        const VulkanRenderPass&) = delete;

    VulkanRenderPass& operator=(
        const VulkanRenderPass&) = delete;

    VulkanRenderPass(
        VulkanRenderPass&& other) noexcept;

    VulkanRenderPass& operator=(
        VulkanRenderPass&& other) noexcept;

    bool create_color(
        const VulkanDevice& device,
        std::uint32_t color_format);

    void destroy() noexcept;

    bool valid() const noexcept {
        return render_pass_ != nullptr;
    }

    void* native_handle() const noexcept {
        return render_pass_;
    }

    std::uint32_t color_format() const noexcept {
        return color_format_;
    }

    const std::string& diagnostic() const noexcept {
        return diagnostic_;
    }

private:
    const VulkanDevice* device_api_{nullptr};
    void* device_{nullptr};
    void* render_pass_{nullptr};
    std::uint32_t color_format_{0};
    std::string diagnostic_{};
};

} // namespace nengine::render
