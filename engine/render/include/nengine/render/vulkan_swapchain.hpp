#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "nengine/render/vulkan_device.hpp"
#include "nengine/render/vulkan_instance.hpp"
#include "nengine/render/vulkan_loader.hpp"
#include "nengine/render/vulkan_surface.hpp"

namespace nengine::render {

class VulkanSwapchain {
public:
    VulkanSwapchain() = default;
    ~VulkanSwapchain();

    VulkanSwapchain(
        const VulkanSwapchain&) = delete;

    VulkanSwapchain& operator=(
        const VulkanSwapchain&) = delete;

    VulkanSwapchain(
        VulkanSwapchain&& other) noexcept;

    VulkanSwapchain& operator=(
        VulkanSwapchain&& other) noexcept;

    bool create(
        const VulkanLoader& loader,
        const VulkanInstance& instance,
        const VulkanDevice& device,
        const VulkanSurface& surface,
        std::uint32_t requested_width,
        std::uint32_t requested_height,
        bool vsync = true);

    void destroy() noexcept;

    bool valid() const noexcept {
        return swapchain_ != nullptr &&
            !images_.empty();
    }

    void* native_handle() const noexcept {
        return swapchain_;
    }

    const std::vector<void*>& images() const noexcept {
        return images_;
    }

    std::uint32_t width() const noexcept {
        return width_;
    }

    std::uint32_t height() const noexcept {
        return height_;
    }

    std::uint32_t format() const noexcept {
        return format_;
    }

    const std::string& diagnostic() const noexcept {
        return diagnostic_;
    }

private:
    using DestroySwapchain =
        void (*)(void*, void*, const void*);

    void* device_{nullptr};
    void* swapchain_{nullptr};
    std::vector<void*> images_{};
    std::uint32_t width_{0};
    std::uint32_t height_{0};
    std::uint32_t format_{0};
    DestroySwapchain destroy_swapchain_{nullptr};
    std::string diagnostic_{};
};

} // namespace nengine::render
