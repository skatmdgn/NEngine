#include "nengine/render/vulkan_depth_target.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace nengine::render {
namespace {

using VkResult = std::int32_t;
using VkDeviceSize = std::uint64_t;

constexpr VkResult VK_SUCCESS = 0;

constexpr std::uint32_t
VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO = 5;

constexpr std::uint32_t
VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO = 14;

constexpr std::uint32_t
VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO = 15;

constexpr std::uint32_t
VK_IMAGE_TYPE_2D = 1;

constexpr std::uint32_t
VK_IMAGE_VIEW_TYPE_2D = 1;

constexpr std::uint32_t
VK_IMAGE_TILING_OPTIMAL = 0;

constexpr std::uint32_t
VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT =
    0x00000020u;

constexpr std::uint32_t
VK_SHARING_MODE_EXCLUSIVE = 0;

constexpr std::uint32_t
VK_SAMPLE_COUNT_1_BIT = 0x00000001u;

constexpr std::uint32_t
VK_IMAGE_LAYOUT_UNDEFINED = 0;

constexpr std::uint32_t
VK_IMAGE_ASPECT_DEPTH_BIT = 0x00000002u;

constexpr std::uint32_t
VK_IMAGE_ASPECT_STENCIL_BIT = 0x00000004u;

constexpr std::uint32_t
VK_COMPONENT_SWIZZLE_IDENTITY = 0;

constexpr std::uint32_t
VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT = 0x00000001u;

constexpr std::uint32_t
VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT =
    0x00000200u;

constexpr std::uint32_t
VK_FORMAT_D32_SFLOAT = 126u;

constexpr std::uint32_t
VK_FORMAT_D24_UNORM_S8_UINT = 129u;

constexpr std::uint32_t
VK_FORMAT_D32_SFLOAT_S8_UINT = 130u;

constexpr std::uint32_t
VK_MAX_MEMORY_TYPES = 32;

constexpr std::uint32_t
VK_MAX_MEMORY_HEAPS = 16;

struct VkExtent3D {
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t depth;
};

struct VkImageCreateInfo {
    std::uint32_t sType;
    const void* pNext;
    std::uint32_t flags;
    std::uint32_t imageType;
    std::uint32_t format;
    VkExtent3D extent;
    std::uint32_t mipLevels;
    std::uint32_t arrayLayers;
    std::uint32_t samples;
    std::uint32_t tiling;
    std::uint32_t usage;
    std::uint32_t sharingMode;
    std::uint32_t queueFamilyIndexCount;
    const std::uint32_t* pQueueFamilyIndices;
    std::uint32_t initialLayout;
};

struct VkMemoryRequirements {
    VkDeviceSize size;
    VkDeviceSize alignment;
    std::uint32_t memoryTypeBits;
};

struct VkMemoryType {
    std::uint32_t propertyFlags;
    std::uint32_t heapIndex;
};

struct VkMemoryHeap {
    VkDeviceSize size;
    std::uint32_t flags;
};

struct VkPhysicalDeviceMemoryProperties {
    std::uint32_t memoryTypeCount;
    VkMemoryType memoryTypes[
        VK_MAX_MEMORY_TYPES];
    std::uint32_t memoryHeapCount;
    VkMemoryHeap memoryHeaps[
        VK_MAX_MEMORY_HEAPS];
};

struct VkMemoryAllocateInfo {
    std::uint32_t sType;
    const void* pNext;
    VkDeviceSize allocationSize;
    std::uint32_t memoryTypeIndex;
};

struct VkComponentMapping {
    std::uint32_t r;
    std::uint32_t g;
    std::uint32_t b;
    std::uint32_t a;
};

struct VkImageSubresourceRange {
    std::uint32_t aspectMask;
    std::uint32_t baseMipLevel;
    std::uint32_t levelCount;
    std::uint32_t baseArrayLayer;
    std::uint32_t layerCount;
};

struct VkImageViewCreateInfo {
    std::uint32_t sType;
    const void* pNext;
    std::uint32_t flags;
    void* image;
    std::uint32_t viewType;
    std::uint32_t format;
    VkComponentMapping components;
    VkImageSubresourceRange subresourceRange;
};

struct VkFormatProperties {
    std::uint32_t linearTilingFeatures;
    std::uint32_t optimalTilingFeatures;
    std::uint32_t bufferFeatures;
};

using GetPhysicalDeviceFormatProperties =
    void (*)(
        void*,
        std::uint32_t,
        VkFormatProperties*);

using CreateImage =
    VkResult (*)(
        void*,
        const VkImageCreateInfo*,
        const void*,
        void**);

using DestroyImage =
    void (*)(
        void*,
        void*,
        const void*);

using GetImageMemoryRequirements =
    void (*)(
        void*,
        void*,
        VkMemoryRequirements*);

using GetPhysicalDeviceMemoryProperties =
    void (*)(
        void*,
        VkPhysicalDeviceMemoryProperties*);

using AllocateMemory =
    VkResult (*)(
        void*,
        const VkMemoryAllocateInfo*,
        const void*,
        void**);

using FreeMemory =
    void (*)(
        void*,
        void*,
        const void*);

using BindImageMemory =
    VkResult (*)(
        void*,
        void*,
        void*,
        VkDeviceSize);

using CreateImageView =
    VkResult (*)(
        void*,
        const VkImageViewCreateInfo*,
        const void*,
        void**);

using DestroyImageView =
    void (*)(
        void*,
        void*,
        const void*);

std::string result_message(
    std::string_view operation,
    VkResult result) {

    return
        std::string{operation} +
        " failed with VkResult " +
        std::to_string(result);
}

template <typename T>
T load_device_proc(
    const VulkanDevice& device,
    const char* name) {

    return reinterpret_cast<T>(
        device.get_proc_address(name));
}

std::optional<std::uint32_t>
find_memory_type(
    const VkPhysicalDeviceMemoryProperties& properties,
    std::uint32_t allowed_bits,
    std::uint32_t required_flags) {

    for (std::uint32_t i = 0;
         i < properties.memoryTypeCount &&
         i < VK_MAX_MEMORY_TYPES;
         ++i) {

        if ((allowed_bits &
             (1u << i)) == 0) {
            continue;
        }

        if ((properties.memoryTypes[i]
                 .propertyFlags &
             required_flags) ==
            required_flags) {
            return i;
        }
    }

    return std::nullopt;
}

bool format_has_stencil(
    std::uint32_t format) noexcept {

    return
        format ==
            VK_FORMAT_D24_UNORM_S8_UINT ||
        format ==
            VK_FORMAT_D32_SFLOAT_S8_UINT;
}

} // namespace

VulkanDepthTarget::~VulkanDepthTarget() {
    destroy();
}

VulkanDepthTarget::VulkanDepthTarget(
    VulkanDepthTarget&& other) noexcept
    : device_api_(
          std::exchange(
              other.device_api_,
              nullptr)),
      device_(
          std::exchange(
              other.device_,
              nullptr)),
      image_(
          std::exchange(
              other.image_,
              nullptr)),
      memory_(
          std::exchange(
              other.memory_,
              nullptr)),
      view_(
          std::exchange(
              other.view_,
              nullptr)),
      format_(
          std::exchange(
              other.format_,
              0)),
      diagnostic_(
          std::move(
              other.diagnostic_)) {}

VulkanDepthTarget&
VulkanDepthTarget::operator=(
    VulkanDepthTarget&& other) noexcept {

    if (this == &other) {
        return *this;
    }

    destroy();

    device_api_ =
        std::exchange(
            other.device_api_,
            nullptr);

    device_ =
        std::exchange(
            other.device_,
            nullptr);

    image_ =
        std::exchange(
            other.image_,
            nullptr);

    memory_ =
        std::exchange(
            other.memory_,
            nullptr);

    view_ =
        std::exchange(
            other.view_,
            nullptr);

    format_ =
        std::exchange(
            other.format_,
            0);

    diagnostic_ =
        std::move(
            other.diagnostic_);

    return *this;
}

bool VulkanDepthTarget::create(
    const VulkanLoader& loader,
    const VulkanInstance& instance,
    const VulkanDevice& device,
    std::uint32_t width,
    std::uint32_t height) {

    destroy();
    diagnostic_.clear();

    if (!loader.loaded() ||
        !instance.valid() ||
        !device.valid() ||
        width == 0 ||
        height == 0) {

        diagnostic_ =
            "valid Vulkan runtime and non-zero depth extent are required";
        return false;
    }

    const auto format_properties =
        reinterpret_cast<
            GetPhysicalDeviceFormatProperties>(
                loader.get_instance_proc_address(
                    instance.native_handle(),
                    "vkGetPhysicalDeviceFormatProperties"));

    const auto memory_properties =
        reinterpret_cast<
            GetPhysicalDeviceMemoryProperties>(
                loader.get_instance_proc_address(
                    instance.native_handle(),
                    "vkGetPhysicalDeviceMemoryProperties"));

    const auto create_image =
        load_device_proc<CreateImage>(
            device,
            "vkCreateImage");

    const auto destroy_image =
        load_device_proc<DestroyImage>(
            device,
            "vkDestroyImage");

    const auto get_requirements =
        load_device_proc<
            GetImageMemoryRequirements>(
                device,
                "vkGetImageMemoryRequirements");

    const auto allocate_memory =
        load_device_proc<AllocateMemory>(
            device,
            "vkAllocateMemory");

    const auto free_memory =
        load_device_proc<FreeMemory>(
            device,
            "vkFreeMemory");

    const auto bind_memory =
        load_device_proc<BindImageMemory>(
            device,
            "vkBindImageMemory");

    const auto create_view =
        load_device_proc<CreateImageView>(
            device,
            "vkCreateImageView");

    if (!format_properties ||
        !memory_properties ||
        !create_image ||
        !destroy_image ||
        !get_requirements ||
        !allocate_memory ||
        !free_memory ||
        !bind_memory ||
        !create_view) {

        diagnostic_ =
            "required Vulkan depth-image functions are unavailable";
        return false;
    }

    const std::uint32_t candidates[] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D32_SFLOAT_S8_UINT
    };

    std::uint32_t chosen_format = 0;

    for (const auto candidate :
         candidates) {

        VkFormatProperties properties{};

        format_properties(
            device.physical_device(),
            candidate,
            &properties);

        if ((properties
                 .optimalTilingFeatures &
             VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0) {

            chosen_format =
                candidate;
            break;
        }
    }

    if (chosen_format == 0) {
        diagnostic_ =
            "no supported Vulkan depth attachment format found";
        return false;
    }

    const VkImageCreateInfo image_info{
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        nullptr,
        0,
        VK_IMAGE_TYPE_2D,
        chosen_format,
        {width, height, 1},
        1,
        1,
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_SHARING_MODE_EXCLUSIVE,
        0,
        nullptr,
        VK_IMAGE_LAYOUT_UNDEFINED
    };

    void* image = nullptr;

    auto status =
        create_image(
            device.native_device(),
            &image_info,
            nullptr,
            &image);

    if (status != VK_SUCCESS ||
        !image) {

        diagnostic_ =
            result_message(
                "vkCreateImage(depth)",
                status);
        return false;
    }

    VkMemoryRequirements requirements{};

    get_requirements(
        device.native_device(),
        image,
        &requirements);

    VkPhysicalDeviceMemoryProperties
        properties{};

    memory_properties(
        device.physical_device(),
        &properties);

    const auto memory_type =
        find_memory_type(
            properties,
            requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (!memory_type) {
        destroy_image(
            device.native_device(),
            image,
            nullptr);

        diagnostic_ =
            "no device-local memory type for Vulkan depth image";
        return false;
    }

    const VkMemoryAllocateInfo allocate_info{
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        nullptr,
        requirements.size,
        *memory_type
    };

    void* memory = nullptr;

    status =
        allocate_memory(
            device.native_device(),
            &allocate_info,
            nullptr,
            &memory);

    if (status != VK_SUCCESS ||
        !memory) {

        destroy_image(
            device.native_device(),
            image,
            nullptr);

        diagnostic_ =
            result_message(
                "vkAllocateMemory(depth)",
                status);
        return false;
    }

    status =
        bind_memory(
            device.native_device(),
            image,
            memory,
            0);

    if (status != VK_SUCCESS) {
        free_memory(
            device.native_device(),
            memory,
            nullptr);

        destroy_image(
            device.native_device(),
            image,
            nullptr);

        diagnostic_ =
            result_message(
                "vkBindImageMemory(depth)",
                status);
        return false;
    }

    std::uint32_t aspect =
        VK_IMAGE_ASPECT_DEPTH_BIT;

    if (format_has_stencil(
            chosen_format)) {

        aspect |=
            VK_IMAGE_ASPECT_STENCIL_BIT;
    }

    const VkImageViewCreateInfo view_info{
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        nullptr,
        0,
        image,
        VK_IMAGE_VIEW_TYPE_2D,
        chosen_format,
        {
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY
        },
        {
            aspect,
            0,
            1,
            0,
            1
        }
    };

    void* view = nullptr;

    status =
        create_view(
            device.native_device(),
            &view_info,
            nullptr,
            &view);

    if (status != VK_SUCCESS ||
        !view) {

        free_memory(
            device.native_device(),
            memory,
            nullptr);

        destroy_image(
            device.native_device(),
            image,
            nullptr);

        diagnostic_ =
            result_message(
                "vkCreateImageView(depth)",
                status);
        return false;
    }

    device_api_ = &device;
    device_ =
        device.native_device();
    image_ = image;
    memory_ = memory;
    view_ = view;
    format_ = chosen_format;

    diagnostic_ =
        "Vulkan depth target created";

    return true;
}

void VulkanDepthTarget::destroy() noexcept {
    if (device_api_ &&
        device_) {

        const auto destroy_view =
            load_device_proc<DestroyImageView>(
                *device_api_,
                "vkDestroyImageView");

        const auto destroy_image =
            load_device_proc<DestroyImage>(
                *device_api_,
                "vkDestroyImage");

        const auto free_memory =
            load_device_proc<FreeMemory>(
                *device_api_,
                "vkFreeMemory");

        if (destroy_view &&
            view_) {
            destroy_view(
                device_,
                view_,
                nullptr);
        }

        if (destroy_image &&
            image_) {
            destroy_image(
                device_,
                image_,
                nullptr);
        }

        if (free_memory &&
            memory_) {
            free_memory(
                device_,
                memory_,
                nullptr);
        }
    }

    device_api_ = nullptr;
    device_ = nullptr;
    image_ = nullptr;
    memory_ = nullptr;
    view_ = nullptr;
    format_ = 0;
}

} // namespace nengine::render
