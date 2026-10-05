#pragma once

#include <string>
#include <vector>

#include "nengine/render/vulkan_instance.hpp"
#include "nengine/render/vulkan_loader.hpp"

namespace nengine::render {

std::vector<std::string>
vulkan_platform_surface_extensions();

class VulkanSurface {
public:
    VulkanSurface() = default;
    ~VulkanSurface();

    VulkanSurface(
        const VulkanSurface&) = delete;

    VulkanSurface& operator=(
        const VulkanSurface&) = delete;

    VulkanSurface(
        VulkanSurface&& other) noexcept;

    VulkanSurface& operator=(
        VulkanSurface&& other) noexcept;

    bool create(
        const VulkanLoader& loader,
        const VulkanInstance& instance,
        void* native_application,
        void* native_window);

    void destroy() noexcept;

    bool valid() const noexcept {
        return surface_ != nullptr;
    }

    void* native_handle() const noexcept {
        return surface_;
    }

    const std::string& diagnostic() const noexcept {
        return diagnostic_;
    }

private:
    using DestroySurface =
        void (*)(void*, void*, const void*);

    void* instance_{nullptr};
    void* surface_{nullptr};
    DestroySurface destroy_surface_{nullptr};
    std::string diagnostic_{};
};

} // namespace nengine::render
