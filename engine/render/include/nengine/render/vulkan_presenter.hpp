#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "nengine/render/vulkan_device.hpp"
#include "nengine/render/vulkan_swapchain.hpp"

namespace nengine::render {

class VulkanClearPresenter {
public:
    VulkanClearPresenter() = default;
    ~VulkanClearPresenter();

    VulkanClearPresenter(
        const VulkanClearPresenter&) = delete;

    VulkanClearPresenter& operator=(
        const VulkanClearPresenter&) = delete;

    bool initialize(
        const VulkanDevice& device,
        const VulkanSwapchain& swapchain);

    bool present_clear(
        float red,
        float green,
        float blue,
        float alpha = 1.0f);

    void shutdown() noexcept;

    bool ready() const noexcept {
        return device_api_ != nullptr &&
            command_pool_ != nullptr &&
            image_available_ != nullptr &&
            render_finished_ != nullptr &&
            frame_fence_ != nullptr &&
            !command_buffers_.empty();
    }

    bool needs_resize() const noexcept {
        return needs_resize_;
    }

    const std::string& diagnostic() const noexcept {
        return diagnostic_;
    }

private:
    const VulkanDevice* device_api_{nullptr};
    void* device_{nullptr};
    void* queue_{nullptr};
    void* swapchain_{nullptr};
    std::vector<void*> images_{};
    std::vector<void*> command_buffers_{};
    std::vector<bool> image_initialized_{};
    void* command_pool_{nullptr};
    void* image_available_{nullptr};
    void* render_finished_{nullptr};
    void* frame_fence_{nullptr};
    bool needs_resize_{false};
    std::string diagnostic_{};
};

} // namespace nengine::render
