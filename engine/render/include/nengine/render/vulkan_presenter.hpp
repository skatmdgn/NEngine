#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "nengine/render/matrix.hpp"
#include "nengine/render/vulkan_device.hpp"
#include "nengine/render/vulkan_mesh.hpp"
#include "nengine/render/vulkan_pipeline.hpp"
#include "nengine/render/vulkan_render_targets.hpp"
#include "nengine/render/vulkan_swapchain.hpp"

namespace nengine::render {

struct VulkanMeshDraw {
    const VulkanGraphicsPipeline* pipeline{nullptr};
    const VulkanMeshResource* mesh{nullptr};
    Mat4 mvp{};
};

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
        const VulkanSwapchain& swapchain,
        const VulkanRenderTargets& targets);

    bool present_clear(
        float red,
        float green,
        float blue,
        float alpha = 1.0f);

    bool present_mesh(
        const VulkanGraphicsPipeline& pipeline,
        const VulkanMeshResource& mesh,
        const Mat4& mvp,
        float clear_red = 0.08f,
        float clear_green = 0.09f,
        float clear_blue = 0.11f,
        float clear_alpha = 1.0f);

    bool present_meshes(
        std::span<const VulkanMeshDraw> draws,
        float clear_red = 0.08f,
        float clear_green = 0.09f,
        float clear_blue = 0.11f,
        float clear_alpha = 1.0f);

    void shutdown() noexcept;

    bool ready() const noexcept {
        return device_api_ != nullptr &&
            targets_ != nullptr &&
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
    bool present_frame(
        std::span<const VulkanMeshDraw> draws,
        float red,
        float green,
        float blue,
        float alpha);

    const VulkanDevice* device_api_{nullptr};
    const VulkanRenderTargets* targets_{nullptr};
    void* device_{nullptr};
    void* queue_{nullptr};
    void* swapchain_{nullptr};
    std::vector<void*> command_buffers_{};
    void* command_pool_{nullptr};
    void* image_available_{nullptr};
    void* render_finished_{nullptr};
    void* frame_fence_{nullptr};
    std::uint32_t width_{0};
    std::uint32_t height_{0};
    bool needs_resize_{false};
    std::string diagnostic_{};
};

} // namespace nengine::render
