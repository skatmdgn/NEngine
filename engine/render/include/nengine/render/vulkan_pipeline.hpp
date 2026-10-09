#pragma once

#include <string>

#include "nengine/render/vulkan_device.hpp"
#include "nengine/render/vulkan_material.hpp"
#include "nengine/render/vulkan_render_pass.hpp"
#include "nengine/render/vulkan_render_targets.hpp"
#include "nengine/render/vulkan_shader.hpp"

namespace nengine::render {

struct VulkanGraphicsPipelineOptions {
    bool depth_test{true};
    bool depth_write{true};
    bool alpha_blend{false};
    bool back_face_culling{true};
};

class VulkanGraphicsPipeline {
public:
    VulkanGraphicsPipeline() = default;
    ~VulkanGraphicsPipeline();

    VulkanGraphicsPipeline(
        const VulkanGraphicsPipeline&) = delete;

    VulkanGraphicsPipeline& operator=(
        const VulkanGraphicsPipeline&) = delete;

    bool create(
        const VulkanDevice& device,
        const VulkanRenderTargets& targets,
        const VulkanShaderModule& vertex_shader,
        const VulkanShaderModule& fragment_shader);

    bool create(
        const VulkanDevice& device,
        const VulkanRenderPass& render_pass,
        const VulkanShaderModule& vertex_shader,
        const VulkanShaderModule& fragment_shader);

    bool create(
        const VulkanDevice& device,
        const VulkanRenderPass& render_pass,
        const VulkanShaderModule& vertex_shader,
        const VulkanShaderModule& fragment_shader,
        const VulkanMaterialResource& material);

    bool create(
        const VulkanDevice& device,
        const VulkanRenderPass& render_pass,
        const VulkanShaderModule& vertex_shader,
        const VulkanShaderModule& fragment_shader,
        const VulkanMaterialResource& material,
        const VulkanGraphicsPipelineOptions& options);

    void destroy() noexcept;

    bool valid() const noexcept {
        return layout_ != nullptr &&
            pipeline_ != nullptr;
    }

    void* native_layout() const noexcept {
        return layout_;
    }

    void* native_pipeline() const noexcept {
        return pipeline_;
    }

    bool supports_material_descriptors() const noexcept {
        return material_descriptor_layout_;
    }

    const std::string& diagnostic() const noexcept {
        return diagnostic_;
    }

private:
    bool create_internal(
        const VulkanDevice& device,
        const VulkanRenderPass& render_pass,
        const VulkanShaderModule& vertex_shader,
        const VulkanShaderModule& fragment_shader,
        void* descriptor_set_layout,
        const VulkanGraphicsPipelineOptions& options);

    const VulkanDevice* device_api_{nullptr};
    void* device_{nullptr};
    void* layout_{nullptr};
    void* pipeline_{nullptr};
    bool material_descriptor_layout_{false};
    std::string diagnostic_{};
};

} // namespace nengine::render
