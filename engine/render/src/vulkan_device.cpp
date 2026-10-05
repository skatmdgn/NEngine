#include "nengine/render/vulkan_device.hpp"

#include <algorithm>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

namespace nengine::render {
namespace {

using VkResult = std::int32_t;
using VulkanFunction = VulkanLoader::Function;

constexpr VkResult VK_SUCCESS = 0;
constexpr VkResult VK_INCOMPLETE = 5;

constexpr std::uint32_t
VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO = 2;

constexpr std::uint32_t
VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO = 3;

constexpr std::uint32_t
VK_QUEUE_GRAPHICS_BIT = 0x00000001u;

struct VkExtent3D {
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t depth;
};

struct VkQueueFamilyProperties {
    std::uint32_t queueFlags;
    std::uint32_t queueCount;
    std::uint32_t timestampValidBits;
    VkExtent3D minImageTransferGranularity;
};

struct VkDeviceQueueCreateInfo {
    std::uint32_t sType;
    const void* pNext;
    std::uint32_t flags;
    std::uint32_t queueFamilyIndex;
    std::uint32_t queueCount;
    const float* pQueuePriorities;
};

struct VkDeviceCreateInfo {
    std::uint32_t sType;
    const void* pNext;
    std::uint32_t flags;
    std::uint32_t queueCreateInfoCount;
    const VkDeviceQueueCreateInfo*
        pQueueCreateInfos;
    std::uint32_t enabledLayerCount;
    const char* const* ppEnabledLayerNames;
    std::uint32_t enabledExtensionCount;
    const char* const* ppEnabledExtensionNames;
    const void* pEnabledFeatures;
};

struct VkExtensionProperties {
    char extensionName[256];
    std::uint32_t specVersion;
};

using EnumeratePhysicalDevices =
    VkResult (*)(
        void*,
        std::uint32_t*,
        void**);

using GetPhysicalDeviceQueueFamilyProperties =
    void (*)(
        void*,
        std::uint32_t*,
        VkQueueFamilyProperties*);

using GetPhysicalDeviceSurfaceSupport =
    VkResult (*)(
        void*,
        std::uint32_t,
        void*,
        std::uint32_t*);

using CreateDevice =
    VkResult (*)(
        void*,
        const VkDeviceCreateInfo*,
        const void*,
        void**);

using GetDeviceProcAddr =
    VulkanFunction (*)(
        void*,
        const char*);

using GetDeviceQueue =
    void (*)(
        void*,
        std::uint32_t,
        std::uint32_t,
        void**);

using EnumerateDeviceExtensionProperties =
    VkResult (*)(
        void*,
        const char*,
        std::uint32_t*,
        VkExtensionProperties*);

std::string result_message(
    std::string_view operation,
    VkResult result) {

    return
        std::string{operation} +
        " failed with VkResult " +
        std::to_string(result);
}

bool contains_extension(
    const std::vector<VulkanDeviceExtensionInfo>&
        available,
    std::string_view name) {

    return std::any_of(
        available.begin(),
        available.end(),
        [name](
            const VulkanDeviceExtensionInfo& extension) {
            return extension.name == name;
        });
}

} // namespace

VulkanDevice::~VulkanDevice() {
    destroy();
}

VulkanDevice::VulkanDevice(
    VulkanDevice&& other) noexcept
    : physical_device_(
          std::exchange(
              other.physical_device_,
              nullptr)),
      device_(
          std::exchange(
              other.device_,
              nullptr)),
      graphics_queue_(
          std::exchange(
              other.graphics_queue_,
              nullptr)),
      graphics_queue_family_(
          std::exchange(
              other.graphics_queue_family_,
              0xFFFFFFFFu)),
      destroy_device_(
          std::exchange(
              other.destroy_device_,
              nullptr)),
      diagnostic_(
          std::move(
              other.diagnostic_)) {}

VulkanDevice& VulkanDevice::operator=(
    VulkanDevice&& other) noexcept {

    if (this == &other) {
        return *this;
    }

    destroy();

    physical_device_ =
        std::exchange(
            other.physical_device_,
            nullptr);

    device_ =
        std::exchange(
            other.device_,
            nullptr);

    graphics_queue_ =
        std::exchange(
            other.graphics_queue_,
            nullptr);

    graphics_queue_family_ =
        std::exchange(
            other.graphics_queue_family_,
            0xFFFFFFFFu);

    destroy_device_ =
        std::exchange(
            other.destroy_device_,
            nullptr);

    diagnostic_ =
        std::move(
            other.diagnostic_);

    return *this;
}

std::vector<VulkanDeviceExtensionInfo>
VulkanDevice::enumerate_extensions(
    const VulkanLoader& loader,
    const VulkanInstance& instance,
    void* physical_device,
    std::string* error) {

    std::vector<VulkanDeviceExtensionInfo> result;

    if (!loader.loaded() ||
        !instance.valid() ||
        !physical_device) {

        if (error) {
            *error =
                "Vulkan instance and physical device are required";
        }

        return result;
    }

    const auto function =
        reinterpret_cast<
            EnumerateDeviceExtensionProperties>(
                loader.get_instance_proc_address(
                    instance.native_handle(),
                    "vkEnumerateDeviceExtensionProperties"));

    if (!function) {
        if (error) {
            *error =
                "vkEnumerateDeviceExtensionProperties is unavailable";
        }
        return result;
    }

    std::uint32_t count = 0;

    auto status =
        function(
            physical_device,
            nullptr,
            &count,
            nullptr);

    if (status != VK_SUCCESS) {
        if (error) {
            *error =
                result_message(
                    "vkEnumerateDeviceExtensionProperties",
                    status);
        }
        return result;
    }

    std::vector<VkExtensionProperties>
        properties(count);

    if (count != 0) {
        status =
            function(
                physical_device,
                nullptr,
                &count,
                properties.data());

        if (status != VK_SUCCESS &&
            status != VK_INCOMPLETE) {

            if (error) {
                *error =
                    result_message(
                        "vkEnumerateDeviceExtensionProperties",
                        status);
            }

            return {};
        }
    }

    properties.resize(count);
    result.reserve(count);

    for (const auto& property :
         properties) {

        result.push_back({
            property.extensionName,
            property.specVersion
        });
    }

    return result;
}

bool VulkanDevice::create(
    const VulkanLoader& loader,
    const VulkanInstance& instance,
    const std::vector<std::string>&
        required_extensions,
    void* presentation_surface) {

    destroy();
    diagnostic_.clear();

    if (!loader.loaded() ||
        !instance.valid()) {

        diagnostic_ =
            "Vulkan loader and instance are required";
        return false;
    }

    const auto enumerate_devices =
        reinterpret_cast<
            EnumeratePhysicalDevices>(
                loader.get_instance_proc_address(
                    instance.native_handle(),
                    "vkEnumeratePhysicalDevices"));

    const auto queue_properties =
        reinterpret_cast<
            GetPhysicalDeviceQueueFamilyProperties>(
                loader.get_instance_proc_address(
                    instance.native_handle(),
                    "vkGetPhysicalDeviceQueueFamilyProperties"));

    const auto surface_support =
        presentation_surface
            ? reinterpret_cast<
                GetPhysicalDeviceSurfaceSupport>(
                    loader.get_instance_proc_address(
                        instance.native_handle(),
                        "vkGetPhysicalDeviceSurfaceSupportKHR"))
            : nullptr;

    const auto create_device =
        reinterpret_cast<CreateDevice>(
            loader.get_instance_proc_address(
                instance.native_handle(),
                "vkCreateDevice"));

    const auto get_device_proc_addr =
        reinterpret_cast<GetDeviceProcAddr>(
            loader.get_instance_proc_address(
                instance.native_handle(),
                "vkGetDeviceProcAddr"));

    if (!enumerate_devices ||
        !queue_properties ||
        !create_device ||
        !get_device_proc_addr ||
        (presentation_surface &&
         !surface_support)) {

        diagnostic_ =
            "required Vulkan device bootstrap functions are unavailable";
        return false;
    }

    std::uint32_t device_count = 0;

    auto status =
        enumerate_devices(
            instance.native_handle(),
            &device_count,
            nullptr);

    if (status != VK_SUCCESS ||
        device_count == 0) {

        diagnostic_ =
            status == VK_SUCCESS
                ? "no Vulkan physical devices are available"
                : result_message(
                    "vkEnumeratePhysicalDevices",
                    status);

        return false;
    }

    std::vector<void*>
        physical_devices(
            device_count);

    status =
        enumerate_devices(
            instance.native_handle(),
            &device_count,
            physical_devices.data());

    if (status != VK_SUCCESS &&
        status != VK_INCOMPLETE) {

        diagnostic_ =
            result_message(
                "vkEnumeratePhysicalDevices",
                status);
        return false;
    }

    physical_devices.resize(
        device_count);

    for (auto* candidate :
         physical_devices) {

        std::uint32_t queue_count = 0;

        queue_properties(
            candidate,
            &queue_count,
            nullptr);

        if (queue_count == 0) {
            continue;
        }

        std::vector<VkQueueFamilyProperties>
            queues(queue_count);

        queue_properties(
            candidate,
            &queue_count,
            queues.data());

        for (std::uint32_t family = 0;
             family < queue_count;
             ++family) {

            if (queues[family].queueCount == 0 ||
                (queues[family].queueFlags &
                 VK_QUEUE_GRAPHICS_BIT) == 0) {
                continue;
            }

            if (presentation_surface) {
                std::uint32_t supported = 0;

                const auto surface_status =
                    surface_support(
                        candidate,
                        family,
                        presentation_surface,
                        &supported);

                if (surface_status != VK_SUCCESS ||
                    supported == 0) {
                    continue;
                }
            }

            std::string extension_error;
            const auto available_extensions =
                enumerate_extensions(
                    loader,
                    instance,
                    candidate,
                    &extension_error);

            if (!extension_error.empty()) {
                continue;
            }

            bool extensions_ok = true;

            for (const auto& required :
                 required_extensions) {

                if (!contains_extension(
                        available_extensions,
                        required)) {

                    extensions_ok = false;
                    break;
                }
            }

            if (!extensions_ok) {
                continue;
            }

            constexpr float priority = 1.0f;

            const VkDeviceQueueCreateInfo
                queue_info{
                    VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    nullptr,
                    0,
                    family,
                    1,
                    &priority
                };

            std::vector<const char*>
                extension_names;

            extension_names.reserve(
                required_extensions.size());

            for (const auto& extension :
                 required_extensions) {
                extension_names.push_back(
                    extension.c_str());
            }

            const VkDeviceCreateInfo
                device_info{
                    VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                    nullptr,
                    0,
                    1,
                    &queue_info,
                    0,
                    nullptr,
                    static_cast<std::uint32_t>(
                        extension_names.size()),
                    extension_names.empty()
                        ? nullptr
                        : extension_names.data(),
                    nullptr
                };

            void* device = nullptr;

            status =
                create_device(
                    candidate,
                    &device_info,
                    nullptr,
                    &device);

            if (status != VK_SUCCESS ||
                !device) {
                continue;
            }

            const auto destroy_device =
                reinterpret_cast<DestroyDevice>(
                    get_device_proc_addr(
                        device,
                        "vkDestroyDevice"));

            const auto get_device_queue =
                reinterpret_cast<GetDeviceQueue>(
                    get_device_proc_addr(
                        device,
                        "vkGetDeviceQueue"));

            if (!destroy_device ||
                !get_device_queue) {

                if (destroy_device) {
                    destroy_device(
                        device,
                        nullptr);
                }

                continue;
            }

            void* queue = nullptr;

            get_device_queue(
                device,
                family,
                0,
                &queue);

            if (!queue) {
                destroy_device(
                    device,
                    nullptr);
                continue;
            }

            physical_device_ =
                candidate;

            device_ =
                device;

            graphics_queue_ =
                queue;

            graphics_queue_family_ =
                family;

            destroy_device_ =
                destroy_device;

            diagnostic_ =
                "Vulkan logical device created";

            return true;
        }
    }

    if (presentation_surface) {
        diagnostic_ =
            "no physical device satisfies graphics presentation and required extensions";
    } else {
        diagnostic_ =
            required_extensions.empty()
                ? "no physical device with a graphics queue could be created"
                : "no physical device satisfies graphics queue and required extensions";
    }

    return false;
}

void VulkanDevice::destroy() noexcept {
    if (device_ &&
        destroy_device_) {

        destroy_device_(
            device_,
            nullptr);
    }

    physical_device_ = nullptr;
    device_ = nullptr;
    graphics_queue_ = nullptr;
    graphics_queue_family_ =
        0xFFFFFFFFu;
    destroy_device_ = nullptr;
}

} // namespace nengine::render
