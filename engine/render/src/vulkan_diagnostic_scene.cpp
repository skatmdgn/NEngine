#include "nengine/render/vulkan_diagnostic_scene.hpp"

#include <cstdint>
#include <span>
#include <vector>

#include "nengine/core/transform.hpp"
#include "nengine/render/builtin_assets.hpp"
#include "nengine/render/diagnostic_shaders.hpp"
#include "nengine/render/matrix.hpp"
#include "nengine/render/render_snapshot.hpp"

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
            diagnostic_textured_vertex_spirv())) {

        diagnostic_ =
            "diagnostic vertex shader failed: " +
            vertex_shader_.diagnostic();

        shutdown();
        return false;
    }

    if (!fragment_shader_.create(
            context.device(),
            VulkanShaderStage::Fragment,
            diagnostic_textured_fragment_spirv())) {

        diagnostic_ =
            "diagnostic fragment shader failed: " +
            fragment_shader_.diagnostic();

        shutdown();
        return false;
    }

    if (!mesh_cache_.initialize(
            context.loader(),
            context.instance(),
            context.device())) {

        diagnostic_ =
            "built-in Vulkan mesh cache failed: " +
            mesh_cache_.diagnostic();

        shutdown();
        return false;
    }

    const std::uint8_t pixels[] = {
        255u,  48u,  48u, 255u,
         48u, 255u,  96u, 255u,
         48u,  96u, 255u, 255u,
        255u, 224u,  48u, 255u
    };

    if (!texture_.create_rgba8(
            context.loader(),
            context.instance(),
            context.device(),
            2u,
            2u,
            pixels,
            sizeof(pixels),
            VulkanTextureColorSpace::SRgb)) {

        diagnostic_ =
            "diagnostic sampled texture failed: " +
            texture_.diagnostic();

        shutdown();
        return false;
    }

    if (!material_.create_textured(
            context.device(),
            texture_)) {

        diagnostic_ =
            "diagnostic textured material failed: " +
            material_.diagnostic();

        shutdown();
        return false;
    }

    if (!pipeline_.create(
            context.device(),
            context.render_targets()
                .render_pass_resource(),
            vertex_shader_,
            fragment_shader_,
            material_)) {

        diagnostic_ =
            "diagnostic textured graphics pipeline failed: " +
            pipeline_.diagnostic();

        shutdown();
        return false;
    }

    diagnostic_ =
        "Vulkan textured preview shaders mesh cache texture material and pipeline ready";

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

    const auto* quad =
        mesh_cache_.find(
            builtin_unit_quad_mesh_guid());

    if (!quad) {
        diagnostic_ =
            "built-in diagnostic quad is unavailable";
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

    const VulkanMeshDraw draw{
        &pipeline_,
        quad,
        mvp,
        &material_
    };

    if (!context.present_meshes(
            std::span<const VulkanMeshDraw>{
                &draw,
                1})) {

        diagnostic_ =
            context.diagnostic();
        return false;
    }

    diagnostic_ =
        "Vulkan diagnostic textured quad presented";

    return true;
}

bool VulkanDiagnosticScene::present_world(
    VulkanContext& context,
    const core::World& world) {

    if (!ready() ||
        !context.ready()) {

        diagnostic_ =
            "Vulkan World preview is not ready";
        return false;
    }

    const auto height =
        context.swapchain().height();

    if (height == 0) {
        diagnostic_ =
            "Vulkan World preview cannot present to zero-height swapchain";
        return false;
    }

    const float aspect =
        static_cast<float>(
            context.swapchain().width()) /
        static_cast<float>(
            height);

    const auto snapshot =
        build_render_snapshot(
            world);

    if (snapshot.cameras.empty()) {
        diagnostic_ =
            "World contains no active Camera; using diagnostic fallback";

        return present(context);
    }

    const auto matrices =
        build_camera_matrices(
            world,
            snapshot.cameras.front().entity,
            aspect);

    if (!matrices) {
        diagnostic_ =
            "active Camera matrices could not be built";
        return false;
    }

    std::vector<VulkanMeshDraw> draws;
    draws.reserve(
        snapshot.meshes.size());

    for (const auto& item :
         snapshot.meshes) {

        const auto* mesh =
            mesh_cache_.find(
                item.renderer.mesh);

        if (!mesh) {
            continue;
        }

        draws.push_back({
            &pipeline_,
            mesh,
            multiply(
                matrices->view_projection,
                item.world),
            &material_
        });
    }

    if (draws.empty()) {
        if (!context.present_clear(
                0.08f,
                0.09f,
                0.11f,
                1.0f)) {

            diagnostic_ =
                context.diagnostic();
            return false;
        }

        diagnostic_ =
            "Vulkan World preview cleared; no supported built-in MeshRenderer items found";
        return true;
    }

    if (!context.present_meshes(
            draws)) {

        diagnostic_ =
            context.diagnostic();
        return false;
    }

    diagnostic_ =
        "Vulkan World preview presented " +
        std::to_string(
            draws.size()) +
        " draw(s)";

    return true;
}

void VulkanDiagnosticScene::shutdown() noexcept {
    pipeline_.destroy();
    material_.destroy();
    texture_.destroy();
    mesh_cache_.shutdown();
    fragment_shader_.destroy();
    vertex_shader_.destroy();
}

} // namespace nengine::render
