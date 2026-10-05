#pragma once

#include <string>

#include "nengine/render/vulkan_context.hpp"
#include "nengine/render/vulkan_mesh.hpp"
#include "nengine/render/vulkan_pipeline.hpp"
#include "nengine/render/vulkan_shader.hpp"

namespace nengine::render {

class VulkanDiagnosticScene {
public:
    VulkanDiagnosticScene() = default;
    ~VulkanDiagnosticScene();

    VulkanDiagnosticScene(
        const VulkanDiagnosticScene&) = delete;

    VulkanDiagnosticScene& operator=(
        const VulkanDiagnosticScene&) = delete;

    bool initialize(
        VulkanContext& context);

    bool present(
        VulkanContext& context);

    void shutdown() noexcept;

    bool ready() const noexcept {
        return vertex_shader_.valid() &&
            fragment_shader_.valid() &&
            mesh_.valid() &&
            pipeline_.valid();
    }

    const std::string& diagnostic() const noexcept {
        return diagnostic_;
    }

private:
    VulkanShaderModule vertex_shader_{};
    VulkanShaderModule fragment_shader_{};
    VulkanMeshResource mesh_{};
    VulkanGraphicsPipeline pipeline_{};
    std::string diagnostic_{};
};

} // namespace nengine::render
