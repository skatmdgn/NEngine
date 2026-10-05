#include "nengine/render/vulkan_context.hpp"

#include <string>
#include <vector>

namespace nengine::render {

VulkanContext::~VulkanContext() {
    shutdown();
}

bool VulkanContext::initialize_for_window(
    void* native_application,
    void* native_window,
    std::uint32_t width,
    std::uint32_t height,
    bool vsync) {

    shutdown();
    diagnostic_.clear();
    vsync_ = vsync;

    if (!loader_.loaded()) {
        diagnostic_ =
            loader_.diagnostic();
        return false;
    }

    const auto instance_extensions =
        vulkan_platform_surface_extensions();

    if (instance_extensions.empty()) {
        diagnostic_ =
            "this platform has no NEngine Vulkan window-surface implementation";
        return false;
    }

    if (!instance_.create(
            loader_,
            "NEngine",
            instance_extensions)) {

        diagnostic_ =
            instance_.diagnostic();
        return false;
    }

    if (!surface_.create(
            loader_,
            instance_,
            native_application,
            native_window)) {

        diagnostic_ =
            surface_.diagnostic();
        shutdown();
        return false;
    }

    if (!device_.create(
            loader_,
            instance_,
            {"VK_KHR_swapchain"},
            surface_.native_handle())) {

        diagnostic_ =
            device_.diagnostic();
        shutdown();
        return false;
    }

    if (!swapchain_.create(
            loader_,
            instance_,
            device_,
            surface_,
            width,
            height,
            vsync_)) {

        diagnostic_ =
            swapchain_.diagnostic();
        shutdown();
        return false;
    }

    if (!render_targets_.create(
            loader_,
            instance_,
            device_,
            swapchain_)) {

        diagnostic_ =
            render_targets_.diagnostic();
        shutdown();
        return false;
    }

    if (!presenter_.initialize(
            device_,
            swapchain_,
            render_targets_)) {

        diagnostic_ =
            presenter_.diagnostic();
        shutdown();
        return false;
    }

    diagnostic_ =
        "Vulkan window context ready";

    return true;
}

bool VulkanContext::resize(
    std::uint32_t width,
    std::uint32_t height) {

    if (!instance_.valid() ||
        !surface_.valid() ||
        !device_.valid()) {

        diagnostic_ =
            "Vulkan context is not initialized";
        return false;
    }

    presenter_.shutdown();
    render_targets_.destroy();

    if (width == 0 ||
        height == 0) {

        swapchain_.destroy();
        diagnostic_ =
            "Vulkan swapchain suspended for zero-sized window";
        return true;
    }

    swapchain_.destroy();

    if (!swapchain_.create(
            loader_,
            instance_,
            device_,
            surface_,
            width,
            height,
            vsync_)) {

        diagnostic_ =
            swapchain_.diagnostic();
        return false;
    }

    if (!render_targets_.create(
            loader_,
            instance_,
            device_,
            swapchain_)) {

        diagnostic_ =
            render_targets_.diagnostic();
        swapchain_.destroy();
        return false;
    }

    if (!presenter_.initialize(
            device_,
            swapchain_,
            render_targets_)) {

        diagnostic_ =
            presenter_.diagnostic();
        render_targets_.destroy();
        swapchain_.destroy();
        return false;
    }

    diagnostic_ =
        "Vulkan swapchain resized";

    return true;
}

bool VulkanContext::present_clear(
    float red,
    float green,
    float blue,
    float alpha) {

    if (!ready()) {
        diagnostic_ =
            "Vulkan context is not ready to present";
        return false;
    }

    if (!presenter_.present_clear(
            red,
            green,
            blue,
            alpha)) {

        diagnostic_ =
            presenter_.diagnostic();
        return false;
    }

    diagnostic_ =
        presenter_.diagnostic();
    return true;
}

bool VulkanContext::present_mesh(
    const VulkanGraphicsPipeline& pipeline,
    const VulkanMeshResource& mesh,
    const Mat4& mvp,
    float clear_red,
    float clear_green,
    float clear_blue,
    float clear_alpha) {

    if (!ready()) {
        diagnostic_ =
            "Vulkan context is not ready to present";
        return false;
    }

    if (!presenter_.present_mesh(
            pipeline,
            mesh,
            mvp,
            clear_red,
            clear_green,
            clear_blue,
            clear_alpha)) {

        diagnostic_ =
            presenter_.diagnostic();
        return false;
    }

    diagnostic_ =
        presenter_.diagnostic();
    return true;
}

void VulkanContext::shutdown() noexcept {
    presenter_.shutdown();
    render_targets_.destroy();
    swapchain_.destroy();
    device_.destroy();
    surface_.destroy();
    instance_.destroy();
}

} // namespace nengine::render
