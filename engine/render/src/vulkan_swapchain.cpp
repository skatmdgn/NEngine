#include "nengine/render/vulkan_swapchain.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string_view>
#include <utility>
#include <vector>

namespace nengine::render {
namespace {

using VkResult = std::int32_t;

constexpr VkResult VK_SUCCESS = 0;
constexpr VkResult VK_INCOMPLETE = 5;

constexpr std::uint32_t
VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR =
    1000001000u;

constexpr std::uint32_t
VK_FORMAT_R8G8B8A8_SRGB = 43u;

constexpr std::uint32_t
VK_FORMAT_B8G8R8A8_SRGB = 50u;

constexpr std::uint32_t
VK_COLOR_SPACE_SRGB_NONLINEAR_KHR = 0u;

constexpr std::uint32_t
VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT =
    0x00000010u;

constexpr std::uint32_t
VK_SHARING_MODE_EXCLUSIVE = 0u;

constexpr std::uint32_t
VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR =
    0x00000001u;

constexpr std::uint32_t
VK_PRESENT_MODE_IMMEDIATE_KHR = 0u;

constexpr std::uint32_t
VK_PRESENT_MODE_MAILBOX_KHR = 1u;

constexpr std::uint32_t
VK_PRESENT_MODE_FIFO_KHR = 2u;

struct VkExtent2D {
    std::uint32_t width;
    std::uint32_t height;
};

struct VkSurfaceCapabilitiesKHR {
    std::uint32_t minImageCount;
    std::uint32_t maxImageCount;
    VkExtent2D currentExtent;
    VkExtent2D minImageExtent;
    VkExtent2D maxImageExtent;
    std::uint32_t maxImageArrayLayers;
    std::uint32_t supportedTransforms;
    std::uint32_t currentTransform;
    std::uint32_t supportedCompositeAlpha;
    std::uint32_t supportedUsageFlags;
};

struct VkSurfaceFormatKHR {
    std::uint32_t format;
    std::uint32_t colorSpace;
};

struct VkSwapchainCreateInfoKHR {
    std::uint32_t sType;
    const void* pNext;
    std::uint32_t flags;
    void* surface;
    std::uint32_t minImageCount;
    std::uint32_t imageFormat;
    std::uint32_t imageColorSpace;
    VkExtent2D imageExtent;
    std::uint32_t imageArrayLayers;
    std::uint32_t imageUsage;
    std::uint32_t imageSharingMode;
    std::uint32_t queueFamilyIndexCount;
    const std::uint32_t* pQueueFamilyIndices;
    std::uint32_t preTransform;
    std::uint32_t compositeAlpha;
    std::uint32_t presentMode;
    std::uint32_t clipped;
    void* oldSwapchain;
};

using GetSurfaceCapabilities =
    VkResult (*)(
        void*,
        void*,
        VkSurfaceCapabilitiesKHR*);

using GetSurfaceFormats =
    VkResult (*)(
        void*,
        void*,
        std::uint32_t*,
        VkSurfaceFormatKHR*);

using GetSurfacePresentModes =
    VkResult (*)(
        void*,
        void*,
        std::uint32_t*,
        std::uint32_t*);

using CreateSwapchain =
    VkResult (*)(
        void*,
        const VkSwapchainCreateInfoKHR*,
        const void*,
        void**);

using GetSwapchainImages =
    VkResult (*)(
        void*,
        void*,
        std::uint32_t*,
        void**);

std::string result_message(
    std::string_view operation,
    VkResult result) {

    return
        std::string{operation} +
        " failed with VkResult " +
        std::to_string(result);
}

std::uint32_t choose_composite_alpha(
    std::uint32_t supported) {

    constexpr std::uint32_t candidates[] = {
        0x00000001u,
        0x00000002u,
        0x00000004u,
        0x00000008u
    };

    for (const auto value :
         candidates) {

        if ((supported & value) != 0) {
            return value;
        }
    }

    return 0;
}

} // namespace

VulkanSwapchain::~VulkanSwapchain() {
    destroy();
}

VulkanSwapchain::VulkanSwapchain(
    VulkanSwapchain&& other) noexcept
    : device_(
          std::exchange(
              other.device_,
              nullptr)),
      swapchain_(
          std::exchange(
              other.swapchain_,
              nullptr)),
      images_(
          std::move(
              other.images_)),
      width_(
          std::exchange(
              other.width_,
              0)),
      height_(
          std::exchange(
              other.height_,
              0)),
      format_(
          std::exchange(
              other.format_,
              0)),
      destroy_swapchain_(
          std::exchange(
              other.destroy_swapchain_,
              nullptr)),
      diagnostic_(
          std::move(
              other.diagnostic_)) {}

VulkanSwapchain& VulkanSwapchain::operator=(
    VulkanSwapchain&& other) noexcept {

    if (this == &other) {
        return *this;
    }

    destroy();

    device_ =
        std::exchange(
            other.device_,
            nullptr);

    swapchain_ =
        std::exchange(
            other.swapchain_,
            nullptr);

    images_ =
        std::move(
            other.images_);

    width_ =
        std::exchange(
            other.width_,
            0);

    height_ =
        std::exchange(
            other.height_,
            0);

    format_ =
        std::exchange(
            other.format_,
            0);

    destroy_swapchain_ =
        std::exchange(
            other.destroy_swapchain_,
            nullptr);

    diagnostic_ =
        std::move(
            other.diagnostic_);

    return *this;
}

bool VulkanSwapchain::create(
    const VulkanLoader& loader,
    const VulkanInstance& instance,
    const VulkanDevice& device,
    const VulkanSurface& surface,
    std::uint32_t requested_width,
    std::uint32_t requested_height,
    bool vsync) {

    destroy();
    diagnostic_.clear();

    if (!loader.loaded() ||
        !instance.valid() ||
        !device.valid() ||
        !surface.valid()) {

        diagnostic_ =
            "Vulkan loader instance device and surface are required";
        return false;
    }

    const auto get_capabilities =
        reinterpret_cast<
            GetSurfaceCapabilities>(
                loader.get_instance_proc_address(
                    instance.native_handle(),
                    "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));

    const auto get_formats =
        reinterpret_cast<GetSurfaceFormats>(
            loader.get_instance_proc_address(
                instance.native_handle(),
                "vkGetPhysicalDeviceSurfaceFormatsKHR"));

    const auto get_present_modes =
        reinterpret_cast<
            GetSurfacePresentModes>(
                loader.get_instance_proc_address(
                    instance.native_handle(),
                    "vkGetPhysicalDeviceSurfacePresentModesKHR"));

    const auto create_swapchain =
        reinterpret_cast<CreateSwapchain>(
            device.get_proc_address(
                "vkCreateSwapchainKHR"));

    const auto destroy_swapchain =
        reinterpret_cast<DestroySwapchain>(
            device.get_proc_address(
                "vkDestroySwapchainKHR"));

    const auto get_images =
        reinterpret_cast<GetSwapchainImages>(
            device.get_proc_address(
                "vkGetSwapchainImagesKHR"));

    if (!get_capabilities ||
        !get_formats ||
        !get_present_modes ||
        !create_swapchain ||
        !destroy_swapchain ||
        !get_images) {

        diagnostic_ =
            "required Vulkan swapchain functions are unavailable";
        return false;
    }

    VkSurfaceCapabilitiesKHR capabilities{};

    auto status =
        get_capabilities(
            device.physical_device(),
            surface.native_handle(),
            &capabilities);

    if (status != VK_SUCCESS) {
        diagnostic_ =
            result_message(
                "vkGetPhysicalDeviceSurfaceCapabilitiesKHR",
                status);
        return false;
    }

    std::uint32_t format_count = 0;

    status =
        get_formats(
            device.physical_device(),
            surface.native_handle(),
            &format_count,
            nullptr);

    if (status != VK_SUCCESS ||
        format_count == 0) {

        diagnostic_ =
            status == VK_SUCCESS
                ? "Vulkan surface exposes no formats"
                : result_message(
                    "vkGetPhysicalDeviceSurfaceFormatsKHR",
                    status);

        return false;
    }

    std::vector<VkSurfaceFormatKHR>
        formats(format_count);

    status =
        get_formats(
            device.physical_device(),
            surface.native_handle(),
            &format_count,
            formats.data());

    if (status != VK_SUCCESS &&
        status != VK_INCOMPLETE) {

        diagnostic_ =
            result_message(
                "vkGetPhysicalDeviceSurfaceFormatsKHR",
                status);
        return false;
    }

    formats.resize(format_count);

    VkSurfaceFormatKHR chosen_format =
        formats.front();

    for (const auto& format :
         formats) {

        if (format.format ==
                VK_FORMAT_B8G8R8A8_SRGB &&
            format.colorSpace ==
                VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {

            chosen_format = format;
            break;
        }

        if (format.format ==
                VK_FORMAT_R8G8B8A8_SRGB &&
            format.colorSpace ==
                VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {

            chosen_format = format;
        }
    }

    std::uint32_t present_count = 0;

    status =
        get_present_modes(
            device.physical_device(),
            surface.native_handle(),
            &present_count,
            nullptr);

    if (status != VK_SUCCESS ||
        present_count == 0) {

        diagnostic_ =
            status == VK_SUCCESS
                ? "Vulkan surface exposes no present modes"
                : result_message(
                    "vkGetPhysicalDeviceSurfacePresentModesKHR",
                    status);

        return false;
    }

    std::vector<std::uint32_t>
        present_modes(
            present_count);

    status =
        get_present_modes(
            device.physical_device(),
            surface.native_handle(),
            &present_count,
            present_modes.data());

    if (status != VK_SUCCESS &&
        status != VK_INCOMPLETE) {

        diagnostic_ =
            result_message(
                "vkGetPhysicalDeviceSurfacePresentModesKHR",
                status);
        return false;
    }

    present_modes.resize(
        present_count);

    std::uint32_t chosen_present =
        VK_PRESENT_MODE_FIFO_KHR;

    if (!vsync) {
        if (std::find(
                present_modes.begin(),
                present_modes.end(),
                VK_PRESENT_MODE_MAILBOX_KHR) !=
            present_modes.end()) {

            chosen_present =
                VK_PRESENT_MODE_MAILBOX_KHR;

        } else if (
            std::find(
                present_modes.begin(),
                present_modes.end(),
                VK_PRESENT_MODE_IMMEDIATE_KHR) !=
            present_modes.end()) {

            chosen_present =
                VK_PRESENT_MODE_IMMEDIATE_KHR;
        }
    }

    VkExtent2D extent{};

    if (capabilities.currentExtent.width !=
        std::numeric_limits<
            std::uint32_t>::max()) {

        extent =
            capabilities.currentExtent;

    } else {
        extent.width =
            std::clamp(
                requested_width,
                capabilities.minImageExtent.width,
                capabilities.maxImageExtent.width);

        extent.height =
            std::clamp(
                requested_height,
                capabilities.minImageExtent.height,
                capabilities.maxImageExtent.height);
    }

    std::uint32_t image_count =
        capabilities.minImageCount + 1;

    if (capabilities.maxImageCount != 0 &&
        image_count >
            capabilities.maxImageCount) {

        image_count =
            capabilities.maxImageCount;
    }

    const auto composite_alpha =
        choose_composite_alpha(
            capabilities.supportedCompositeAlpha);

    if (composite_alpha == 0) {
        diagnostic_ =
            "Vulkan surface exposes no supported composite alpha mode";
        return false;
    }

    constexpr std::uint32_t image_usage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    if ((capabilities.supportedUsageFlags &
         image_usage) != image_usage) {

        diagnostic_ =
            "Vulkan surface does not support color-attachment swapchain images";
        return false;
    }

    const VkSwapchainCreateInfoKHR info{
        VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        nullptr,
        0,
        surface.native_handle(),
        image_count,
        chosen_format.format,
        chosen_format.colorSpace,
        extent,
        1,
        image_usage,
        VK_SHARING_MODE_EXCLUSIVE,
        0,
        nullptr,
        capabilities.currentTransform,
        composite_alpha,
        chosen_present,
        1,
        nullptr
    };

    void* swapchain = nullptr;

    status =
        create_swapchain(
            device.native_device(),
            &info,
            nullptr,
            &swapchain);

    if (status != VK_SUCCESS ||
        !swapchain) {

        diagnostic_ =
            result_message(
                "vkCreateSwapchainKHR",
                status);
        return false;
    }

    std::uint32_t actual_count = 0;

    status =
        get_images(
            device.native_device(),
            swapchain,
            &actual_count,
            nullptr);

    if (status != VK_SUCCESS ||
        actual_count == 0) {

        destroy_swapchain(
            device.native_device(),
            swapchain,
            nullptr);

        diagnostic_ =
            status == VK_SUCCESS
                ? "created swapchain exposes no images"
                : result_message(
                    "vkGetSwapchainImagesKHR",
                    status);

        return false;
    }

    std::vector<void*> images(
        actual_count);

    status =
        get_images(
            device.native_device(),
            swapchain,
            &actual_count,
            images.data());

    if (status != VK_SUCCESS &&
        status != VK_INCOMPLETE) {

        destroy_swapchain(
            device.native_device(),
            swapchain,
            nullptr);

        diagnostic_ =
            result_message(
                "vkGetSwapchainImagesKHR",
                status);

        return false;
    }

    images.resize(
        actual_count);

    device_ =
        device.native_device();

    swapchain_ =
        swapchain;

    images_ =
        std::move(images);

    width_ =
        extent.width;

    height_ =
        extent.height;

    format_ =
        chosen_format.format;

    destroy_swapchain_ =
        destroy_swapchain;

    diagnostic_ =
        "Vulkan swapchain created";

    return true;
}

void VulkanSwapchain::destroy() noexcept {
    if (device_ &&
        swapchain_ &&
        destroy_swapchain_) {

        destroy_swapchain_(
            device_,
            swapchain_,
            nullptr);
    }

    device_ = nullptr;
    swapchain_ = nullptr;
    images_.clear();
    width_ = 0;
    height_ = 0;
    format_ = 0;
    destroy_swapchain_ = nullptr;
}

} // namespace nengine::render
