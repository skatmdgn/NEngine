#pragma once

#include <string>
#include <vector>

#include "nengine/render/vulkan_device.hpp"
#include "nengine/render/vulkan_swapchain.hpp"

namespace nengine::render {

class VulkanRenderTargets {
public:
    VulkanRenderTargets() = default;
    ~VulkanRenderTargets();

    VulkanRenderTargets(
        const VulkanRenderTargets&) = delete;

    VulkanRenderTargets& operator=(
        const VulkanRenderTargets&) = delete;

    bool create(
        const VulkanDevice& device,
        const VulkanSwapchain& swapchain);

    void destroy() noexcept;

    bool valid() const noexcept {
        return render_pass_ != nullptr &&
            !image_views_.empty() &&
            framebuffers_.size() ==
                image_views_.size();
    }

    void* render_pass() const noexcept {
        return render_pass_;
    }

    void* framebuffer(
        std::size_t index) const noexcept {
        return index < framebuffers_.size()
            ? framebuffers_[index]
            : nullptr;
    }

    std::size_t count() const noexcept {
        return framebuffers_.size();
    }

    const std::string& diagnostic() const noexcept {
        return diagnostic_;
    }

private:
    const VulkanDevice* device_api_{nullptr};
    void* device_{nullptr};
    void* render_pass_{nullptr};
    std::vector<void*> image_views_{};
    std::vector<void*> framebuffers_{};
    std::string diagnostic_{};
};

} // namespace nengine::render
