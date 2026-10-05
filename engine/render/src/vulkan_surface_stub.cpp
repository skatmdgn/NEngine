#include "nengine/render/vulkan_surface.hpp"

#include <utility>

namespace nengine::render {

std::vector<std::string>
vulkan_platform_surface_extensions() {
    return {};
}

VulkanSurface::~VulkanSurface() {
    destroy();
}

VulkanSurface::VulkanSurface(
    VulkanSurface&& other) noexcept
    : instance_(
          std::exchange(
              other.instance_,
              nullptr)),
      surface_(
          std::exchange(
              other.surface_,
              nullptr)),
      destroy_surface_(
          std::exchange(
              other.destroy_surface_,
              nullptr)),
      diagnostic_(
          std::move(
              other.diagnostic_)) {}

VulkanSurface& VulkanSurface::operator=(
    VulkanSurface&& other) noexcept {

    if (this == &other) {
        return *this;
    }

    destroy();

    instance_ =
        std::exchange(
            other.instance_,
            nullptr);

    surface_ =
        std::exchange(
            other.surface_,
            nullptr);

    destroy_surface_ =
        std::exchange(
            other.destroy_surface_,
            nullptr);

    diagnostic_ =
        std::move(
            other.diagnostic_);

    return *this;
}

bool VulkanSurface::create(
    const VulkanLoader&,
    const VulkanInstance&,
    void*,
    void*) {

    destroy();

    diagnostic_ =
        "Vulkan window surface is not implemented for this platform";

    return false;
}

void VulkanSurface::destroy() noexcept {
    instance_ = nullptr;
    surface_ = nullptr;
    destroy_surface_ = nullptr;
}

} // namespace nengine::render
