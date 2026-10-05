#include "nengine/render/vulkan_surface.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstdint>
#include <utility>

namespace nengine::render {
namespace {

using VkResult = std::int32_t;

constexpr VkResult VK_SUCCESS = 0;

constexpr std::uint32_t
VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR =
    1000009000u;

struct VkWin32SurfaceCreateInfoKHR {
    std::uint32_t sType;
    const void* pNext;
    std::uint32_t flags;
    HINSTANCE hinstance;
    HWND hwnd;
};

using CreateWin32Surface =
    VkResult (*)(
        void*,
        const VkWin32SurfaceCreateInfoKHR*,
        const void*,
        void**);

} // namespace

std::vector<std::string>
vulkan_platform_surface_extensions() {
    return {
        "VK_KHR_surface",
        "VK_KHR_win32_surface"
    };
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
    const VulkanLoader& loader,
    const VulkanInstance& instance,
    void* native_application,
    void* native_window) {

    destroy();
    diagnostic_.clear();

    if (!loader.loaded() ||
        !instance.valid() ||
        !native_application ||
        !native_window) {

        diagnostic_ =
            "Vulkan loader, instance, HINSTANCE and HWND are required";
        return false;
    }

    const auto create_surface =
        reinterpret_cast<CreateWin32Surface>(
            loader.get_instance_proc_address(
                instance.native_handle(),
                "vkCreateWin32SurfaceKHR"));

    const auto destroy_surface =
        reinterpret_cast<DestroySurface>(
            loader.get_instance_proc_address(
                instance.native_handle(),
                "vkDestroySurfaceKHR"));

    if (!create_surface ||
        !destroy_surface) {

        diagnostic_ =
            "required Win32 Vulkan surface functions are unavailable";
        return false;
    }

    const VkWin32SurfaceCreateInfoKHR info{
        VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        nullptr,
        0,
        static_cast<HINSTANCE>(
            native_application),
        static_cast<HWND>(
            native_window)
    };

    void* surface = nullptr;

    const auto status =
        create_surface(
            instance.native_handle(),
            &info,
            nullptr,
            &surface);

    if (status != VK_SUCCESS ||
        !surface) {

        diagnostic_ =
            "vkCreateWin32SurfaceKHR failed with VkResult " +
            std::to_string(status);
        return false;
    }

    instance_ =
        instance.native_handle();

    surface_ = surface;

    destroy_surface_ =
        destroy_surface;

    diagnostic_ =
        "Win32 Vulkan surface created";

    return true;
}

void VulkanSurface::destroy() noexcept {
    if (instance_ &&
        surface_ &&
        destroy_surface_) {

        destroy_surface_(
            instance_,
            surface_,
            nullptr);
    }

    instance_ = nullptr;
    surface_ = nullptr;
    destroy_surface_ = nullptr;
}

} // namespace nengine::render
