#include "nengine/render/vulkan_diagnostic_scene.hpp"

#include "nengine/core/transform.hpp"
#include "nengine/render/diagnostic_shaders.hpp"
#include "nengine/render/matrix.hpp"
#include "nengine/render/mesh_data.hpp"

namespace nengine::render {

VulkanDiagnosticScene::~VulkanDiagnosticScene() {
    shutdown();
}

bool VulkanDiagnosticScene::initialize(
    VulkanContext& context) {

    shutdown();
    diagnostic_.clear();

    if (!context.ready()) {
        diagnostic_ =
            "Vulkan context is not ready";
        return false;
    }

    if (!vertex_shader_.create(
            context.device(),
            VulkanShaderStage::Vertex,
            diagnostic_vertex_spirv())) {

        diagnostic_ =
            "diagnostic vertex shader failed: " +
            vertex_shader_.diagnostic();

        shutdown();
        return false;
    }

    if (!fragment_shader_.create(
            context.device(),
            VulkanShaderStage::Fragment,
            diagnostic_fragment_spirv())) {

        diagnostic_ =
            "diagnostic fragment shader failed: " +
            fragment_shader_.diagnostic();

        shutdown();
        return false;
    }

    const auto cpu_mesh =
        make_unit_quad_mesh();

    if (!mesh_.create(
            context.loader(),
            context.instance(),
            context.device(),
            cpu_mesh)) {

        diagnostic_ =
            "diagnostic GPU mesh failed: " +
            mesh_.diagnostic();

        shutdown();
        return false;
    }

    if (!pipeline_.create(
            context.device(),
            context.render_targets(),
            vertex_shader_,
            fragment_shader_)) {

        diagnostic_ =
            "diagnostic graphics pipeline failed: " +
            pipeline_.diagnostic();

        shutdown();
        return false;
    }

    diagnostic_ =
        "Vulkan diagnostic indexed-draw resources ready";

    return true;
}

bool VulkanDiagnosticScene::present(
    VulkanContext& context) {

    if (!ready() ||
        !context.ready()) {

        diagnostic_ =
            "Vulkan diagnostic scene is not ready";
        return false;
    }

    const auto height =
        context.swapchain().height();

    if (height == 0) {
        diagnostic_ =
            "Vulkan diagnostic scene cannot present to zero-height swapchain";
        return false;
    }

    const float aspect =
        static_cast<float>(
            context.swapchain().width()) /
        static_cast<float>(
            height);

    core::Transform model_transform;
    model_transform.local_position =
        {0.0f, 0.0f, 2.0f};

    const auto model =
        transform_matrix(
            model_transform);

    const auto projection =
        perspective_lh_zo(
            60.0f,
            aspect,
            0.1f,
            100.0f);

    const auto mvp =
        multiply(
            projection,
            model);

    if (!context.present_mesh(
            pipeline_,
            mesh_,
            mvp)) {

        diagnostic_ =
            context.diagnostic();
        return false;
    }

    diagnostic_ =
        "Vulkan diagnostic indexed mesh presented";

    return true;
}

void VulkanDiagnosticScene::shutdown() noexcept {
    pipeline_.destroy();
    mesh_.destroy();
    fragment_shader_.destroy();
    vertex_shader_.destroy();
}

} // namespace nengine::render
