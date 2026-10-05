#pragma once

#include <cstdint>
#include <string>

#include "nengine/render/vulkan_device.hpp"
#include "nengine/render/vulkan_instance.hpp"
#include "nengine/render/vulkan_loader.hpp"
#include "nengine/render/vulkan_presenter.hpp"
#include "nengine/render/vulkan_surface.hpp"
#include "nengine/render/vulkan_swapchain.hpp"

namespace nengine::render {

class VulkanContext {
public:
    VulkanContext() = default;
    ~VulkanContext();

    VulkanContext(
        const VulkanContext&) = delete;

    VulkanContext& operator=(
        const VulkanContext&) = delete;

    bool initialize_for_window(
        void* native_application,
        void* native_window,
        std::uint32_t width,
        std::uint32_t height,
        bool vsync = true);

    bool resize(
        std::uint32_t width,
        std::uint32_t height);

    bool present_clear(
        float red,
        float green,
        float blue,
        float alpha = 1.0f);

    void shutdown() noexcept;

    bool ready() const noexcept {
        return swapchain_.valid() &&
            presenter_.ready();
    }

    const VulkanLoader& loader() const noexcept {
        return loader_;
    }

    const VulkanInstance& instance() const noexcept {
        return instance_;
    }

    const VulkanSurface& surface() const noexcept {
        return surface_;
    }

    const VulkanDevice& device() const noexcept {
        return device_;
    }

    const VulkanSwapchain& swapchain() const noexcept {
        return swapchain_;
    }

    const std::string& diagnostic() const noexcept {
        return diagnostic_;
    }

private:
    VulkanLoader loader_{};
    VulkanInstance instance_{};
    VulkanSurface surface_{};
    VulkanDevice device_{};
    VulkanSwapchain swapchain_{};
    VulkanClearPresenter presenter_{};
    bool vsync_{true};
    std::string diagnostic_{};
};

} // namespace nengine::render
