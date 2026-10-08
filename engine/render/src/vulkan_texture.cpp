#include "nengine/render/vulkan_texture.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "nengine/render/vulkan_buffer.hpp"

namespace nengine::render {
namespace {

using VkResult = std::int32_t;
using VkDeviceSize = std::uint64_t;

constexpr VkResult VK_SUCCESS = 0;

constexpr std::uint32_t
VK_STRUCTURE_TYPE_SUBMIT_INFO = 4;
constexpr std::uint32_t
VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO = 5;
constexpr std::uint32_t
VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO = 14;
constexpr std::uint32_t
VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO = 15;
constexpr std::uint32_t
VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO = 31;
constexpr std::uint32_t
VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO = 39;
constexpr std::uint32_t
VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO = 40;
constexpr std::uint32_t
VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO = 42;
constexpr std::uint32_t
VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER = 45;

constexpr std::uint32_t
VK_IMAGE_TYPE_2D = 1;
constexpr std::uint32_t
VK_IMAGE_VIEW_TYPE_2D = 1;
constexpr std::uint32_t
VK_IMAGE_TILING_OPTIMAL = 0;
constexpr std::uint32_t
VK_IMAGE_USAGE_TRANSFER_DST_BIT = 0x00000002u;
constexpr std::uint32_t
VK_IMAGE_USAGE_SAMPLED_BIT = 0x00000004u;
constexpr std::uint32_t
VK_SHARING_MODE_EXCLUSIVE = 0;
constexpr std::uint32_t
VK_SAMPLE_COUNT_1_BIT = 0x00000001u;

constexpr std::uint32_t
VK_IMAGE_LAYOUT_UNDEFINED = 0;
constexpr std::uint32_t
VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL = 5;
constexpr std::uint32_t
VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL = 7;

constexpr std::uint32_t
VK_ACCESS_SHADER_READ_BIT = 0x00000020u;
constexpr std::uint32_t
VK_ACCESS_TRANSFER_WRITE_BIT = 0x00001000u;

constexpr std::uint32_t
VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT = 0x00000001u;
constexpr std::uint32_t
VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT = 0x00000080u;
constexpr std::uint32_t
VK_PIPELINE_STAGE_TRANSFER_BIT = 0x00001000u;

constexpr std::uint32_t
VK_IMAGE_ASPECT_COLOR_BIT = 0x00000001u;

constexpr std::uint32_t
VK_QUEUE_FAMILY_IGNORED = 0xFFFFFFFFu;

constexpr std::uint32_t
VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT = 0x00000001u;

constexpr std::uint32_t
VK_FORMAT_R8G8B8A8_UNORM = 37u;
constexpr std::uint32_t
VK_FORMAT_R8G8B8A8_SRGB = 43u;

constexpr std::uint32_t
VK_COMPONENT_SWIZZLE_IDENTITY = 0;

constexpr std::uint32_t
VK_FILTER_LINEAR = 1u;
constexpr std::uint32_t
VK_SAMPLER_MIPMAP_MODE_LINEAR = 1u;
constexpr std::uint32_t
VK_SAMPLER_ADDRESS_MODE_REPEAT = 0u;
constexpr std::uint32_t
VK_COMPARE_OP_ALWAYS = 7u;
constexpr std::uint32_t
VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK = 0u;

constexpr std::uint32_t
VK_COMMAND_POOL_CREATE_TRANSIENT_BIT = 0x00000001u;
constexpr std::uint32_t
VK_COMMAND_BUFFER_LEVEL_PRIMARY = 0u;
constexpr std::uint32_t
VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT = 0x00000001u;

constexpr std::uint32_t
VK_MAX_MEMORY_TYPES = 32;
constexpr std::uint32_t
VK_MAX_MEMORY_HEAPS = 16;

struct VkExtent3D {
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t depth;
};

struct VkOffset3D {
    std::int32_t x;
    std::int32_t y;
    std::int32_t z;
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

struct VkSamplerCreateInfo {
    std::uint32_t sType;
    const void* pNext;
    std::uint32_t flags;
    std::uint32_t magFilter;
    std::uint32_t minFilter;
    std::uint32_t mipmapMode;
    std::uint32_t addressModeU;
    std::uint32_t addressModeV;
    std::uint32_t addressModeW;
    float mipLodBias;
    std::uint32_t anisotropyEnable;
    float maxAnisotropy;
    std::uint32_t compareEnable;
    std::uint32_t compareOp;
    float minLod;
    float maxLod;
    std::uint32_t borderColor;
    std::uint32_t unnormalizedCoordinates;
};

struct VkCommandPoolCreateInfo {
    std::uint32_t sType;
    const void* pNext;
    std::uint32_t flags;
    std::uint32_t queueFamilyIndex;
};

struct VkCommandBufferAllocateInfo {
    std::uint32_t sType;
    const void* pNext;
    void* commandPool;
    std::uint32_t level;
    std::uint32_t commandBufferCount;
};

struct VkCommandBufferBeginInfo {
    std::uint32_t sType;
    const void* pNext;
    std::uint32_t flags;
    const void* pInheritanceInfo;
};

struct VkImageSubresourceLayers {
    std::uint32_t aspectMask;
    std::uint32_t mipLevel;
    std::uint32_t baseArrayLayer;
    std::uint32_t layerCount;
};

struct VkBufferImageCopy {
    VkDeviceSize bufferOffset;
    std::uint32_t bufferRowLength;
    std::uint32_t bufferImageHeight;
    VkImageSubresourceLayers imageSubresource;
    VkOffset3D imageOffset;
    VkExtent3D imageExtent;
};

struct VkImageMemoryBarrier {
    std::uint32_t sType;
    const void* pNext;
    std::uint32_t srcAccessMask;
    std::uint32_t dstAccessMask;
    std::uint32_t oldLayout;
    std::uint32_t newLayout;
    std::uint32_t srcQueueFamilyIndex;
    std::uint32_t dstQueueFamilyIndex;
    void* image;
    VkImageSubresourceRange subresourceRange;
};

struct VkSubmitInfo {
    std::uint32_t sType;
    const void* pNext;
    std::uint32_t waitSemaphoreCount;
    void* const* pWaitSemaphores;
    const std::uint32_t* pWaitDstStageMask;
    std::uint32_t commandBufferCount;
    void* const* pCommandBuffers;
    std::uint32_t signalSemaphoreCount;
    void* const* pSignalSemaphores;
};

using CreateImage =
    VkResult (*)(void*, const VkImageCreateInfo*, const void*, void**);
using DestroyImage =
    void (*)(void*, void*, const void*);
using GetImageMemoryRequirements =
    void (*)(void*, void*, VkMemoryRequirements*);
using GetPhysicalDeviceMemoryProperties =
    void (*)(void*, VkPhysicalDeviceMemoryProperties*);
using AllocateMemory =
    VkResult (*)(void*, const VkMemoryAllocateInfo*, const void*, void**);
using FreeMemory =
    void (*)(void*, void*, const void*);
using BindImageMemory =
    VkResult (*)(void*, void*, void*, VkDeviceSize);
using CreateImageView =
    VkResult (*)(void*, const VkImageViewCreateInfo*, const void*, void**);
using DestroyImageView =
    void (*)(void*, void*, const void*);
using CreateSampler =
    VkResult (*)(void*, const VkSamplerCreateInfo*, const void*, void**);
using DestroySampler =
    void (*)(void*, void*, const void*);
using CreateCommandPool =
    VkResult (*)(void*, const VkCommandPoolCreateInfo*, const void*, void**);
using DestroyCommandPool =
    void (*)(void*, void*, const void*);
using AllocateCommandBuffers =
    VkResult (*)(void*, const VkCommandBufferAllocateInfo*, void**);
using BeginCommandBuffer =
    VkResult (*)(void*, const VkCommandBufferBeginInfo*);
using EndCommandBuffer =
    VkResult (*)(void*);
using CmdPipelineBarrier =
    void (*)(
        void*,
        std::uint32_t,
        std::uint32_t,
        std::uint32_t,
        std::uint32_t,
        const void*,
        std::uint32_t,
        const void*,
        std::uint32_t,
        const VkImageMemoryBarrier*);
using CmdCopyBufferToImage =
    void (*)(
        void*,
        void*,
        void*,
        std::uint32_t,
        std::uint32_t,
        const VkBufferImageCopy*);
using QueueSubmit =
    VkResult (*)(void*, std::uint32_t, const VkSubmitInfo*, void*);
using QueueWaitIdle =
    VkResult (*)(void*);

std::string result_message(
    std::string_view operation,
    VkResult result) {

    return std::string{operation} +
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

        if ((allowed_bits & (1u << i)) == 0) {
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

struct Rgba8MipLevel {
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::uint64_t offset{0};
};

struct Rgba8MipChain {
    std::vector<std::uint8_t> bytes{};
    std::vector<Rgba8MipLevel> levels{};
};

double srgb_to_linear(
    std::uint8_t value) noexcept {

    const double encoded =
        static_cast<double>(
            value) /
        255.0;

    return encoded <= 0.04045
        ? encoded / 12.92
        : std::pow(
            (encoded + 0.055) /
                1.055,
            2.4);
}

std::uint8_t linear_to_srgb(
    double value) noexcept {

    value =
        std::clamp(
            value,
            0.0,
            1.0);

    const double encoded =
        value <= 0.0031308
            ? value * 12.92
            : 1.055 *
                std::pow(
                    value,
                    1.0 / 2.4) -
                0.055;

    return static_cast<std::uint8_t>(
        std::lround(
            std::clamp(
                encoded,
                0.0,
                1.0) *
            255.0));
}

bool build_rgba8_mip_chain(
    std::uint32_t width,
    std::uint32_t height,
    const void* pixels,
    std::size_t pixel_bytes,
    VulkanTextureColorSpace color_space,
    Rgba8MipChain& chain,
    std::string& diagnostic) {

    chain = {};

    const auto base_bytes =
        static_cast<std::uint64_t>(
            width) *
        static_cast<std::uint64_t>(
            height) *
        4u;

    if (width == 0u ||
        height == 0u ||
        !pixels ||
        base_bytes !=
            static_cast<std::uint64_t>(
                pixel_bytes) ||
        base_bytes >
            std::numeric_limits<
                std::size_t>::max()) {

        diagnostic =
            "invalid RGBA8 base image for mip generation";
        return false;
    }

    std::uint32_t mip_count = 1u;

    for (auto w = width,
              h = height;
         w > 1u || h > 1u;) {

        w =
            std::max(
                1u,
                w / 2u);

        h =
            std::max(
                1u,
                h / 2u);

        ++mip_count;
    }

    std::uint64_t total_bytes = 0u;
    std::uint32_t level_width =
        width;
    std::uint32_t level_height =
        height;

    chain.levels.reserve(
        mip_count);

    for (std::uint32_t level = 0u;
         level < mip_count;
         ++level) {

        const auto level_bytes =
            static_cast<std::uint64_t>(
                level_width) *
            static_cast<std::uint64_t>(
                level_height) *
            4u;

        if (level_bytes >
                std::numeric_limits<
                    std::size_t>::max() ||
            total_bytes >
                std::numeric_limits<
                    std::size_t>::max() -
                    level_bytes) {

            diagnostic =
                "RGBA8 mip chain exceeds addressable memory";
            return false;
        }

        chain.levels.push_back({
            level_width,
            level_height,
            total_bytes
        });

        total_bytes +=
            level_bytes;

        level_width =
            std::max(
                1u,
                level_width / 2u);

        level_height =
            std::max(
                1u,
                level_height / 2u);
    }

    chain.bytes.resize(
        static_cast<std::size_t>(
            total_bytes));

    std::copy_n(
        static_cast<
            const std::uint8_t*>(
                pixels),
        pixel_bytes,
        chain.bytes.data());

    for (std::size_t level = 1u;
         level < chain.levels.size();
         ++level) {

        const auto& source =
            chain.levels[level - 1u];

        const auto& destination =
            chain.levels[level];

        const auto* source_pixels =
            chain.bytes.data() +
            static_cast<std::size_t>(
                source.offset);

        auto* destination_pixels =
            chain.bytes.data() +
            static_cast<std::size_t>(
                destination.offset);

        for (std::uint32_t y = 0u;
             y < destination.height;
             ++y) {

            for (std::uint32_t x = 0u;
                 x < destination.width;
                 ++x) {

                double rgb[3]{
                    0.0,
                    0.0,
                    0.0};

                double alpha = 0.0;
                std::uint32_t samples = 0u;

                for (std::uint32_t oy = 0u;
                     oy < 2u;
                     ++oy) {

                    const auto sy =
                        y * 2u + oy;

                    if (sy >=
                        source.height) {
                        continue;
                    }

                    for (std::uint32_t ox = 0u;
                         ox < 2u;
                         ++ox) {

                        const auto sx =
                            x * 2u + ox;

                        if (sx >=
                            source.width) {
                            continue;
                        }

                        const auto index =
                            (static_cast<
                                std::size_t>(
                                    sy) *
                                source.width +
                             sx) *
                            4u;

                        for (std::size_t channel = 0u;
                             channel < 3u;
                             ++channel) {

                            rgb[channel] +=
                                color_space ==
                                    VulkanTextureColorSpace::SRgb
                                    ? srgb_to_linear(
                                        source_pixels[
                                            index +
                                            channel])
                                    : static_cast<double>(
                                        source_pixels[
                                            index +
                                            channel]) /
                                        255.0;
                        }

                        alpha +=
                            static_cast<double>(
                                source_pixels[
                                    index + 3u]) /
                            255.0;

                        ++samples;
                    }
                }

                const auto output =
                    (static_cast<
                        std::size_t>(
                            y) *
                        destination.width +
                     x) *
                    4u;

                const double divisor =
                    samples != 0u
                        ? static_cast<double>(
                            samples)
                        : 1.0;

                for (std::size_t channel = 0u;
                     channel < 3u;
                     ++channel) {

                    const auto averaged =
                        rgb[channel] /
                        divisor;

                    destination_pixels[
                        output +
                        channel] =
                        color_space ==
                            VulkanTextureColorSpace::SRgb
                            ? linear_to_srgb(
                                averaged)
                            : static_cast<
                                std::uint8_t>(
                                    std::lround(
                                        std::clamp(
                                            averaged,
                                            0.0,
                                            1.0) *
                                        255.0));
                }

                destination_pixels[
                    output + 3u] =
                    static_cast<
                        std::uint8_t>(
                            std::lround(
                                std::clamp(
                                    alpha /
                                        divisor,
                                    0.0,
                                    1.0) *
                                255.0));
            }
        }
    }

    diagnostic =
        "RGBA8 mip chain generated";

    return true;
}

bool upload_image_sync(
    const VulkanDevice& device,
    void* staging_buffer,
    void* image,
    std::span<const Rgba8MipLevel> levels,
    std::string& diagnostic) {

    const auto create_pool =
        load_device_proc<CreateCommandPool>(
            device, "vkCreateCommandPool");
    const auto destroy_pool =
        load_device_proc<DestroyCommandPool>(
            device, "vkDestroyCommandPool");
    const auto allocate_commands =
        load_device_proc<AllocateCommandBuffers>(
            device, "vkAllocateCommandBuffers");
    const auto begin_command =
        load_device_proc<BeginCommandBuffer>(
            device, "vkBeginCommandBuffer");
    const auto end_command =
        load_device_proc<EndCommandBuffer>(
            device, "vkEndCommandBuffer");
    const auto pipeline_barrier =
        load_device_proc<CmdPipelineBarrier>(
            device, "vkCmdPipelineBarrier");
    const auto copy_buffer_to_image =
        load_device_proc<CmdCopyBufferToImage>(
            device, "vkCmdCopyBufferToImage");
    const auto queue_submit =
        load_device_proc<QueueSubmit>(
            device, "vkQueueSubmit");
    const auto queue_wait_idle =
        load_device_proc<QueueWaitIdle>(
            device, "vkQueueWaitIdle");

    if (!create_pool || !destroy_pool ||
        !allocate_commands || !begin_command ||
        !end_command || !pipeline_barrier ||
        !copy_buffer_to_image || !queue_submit ||
        !queue_wait_idle) {

        diagnostic =
            "required Vulkan texture upload functions are unavailable";
        return false;
    }

    const VkCommandPoolCreateInfo pool_info{
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        nullptr,
        VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
        device.graphics_queue_family()
    };

    void* pool = nullptr;
    auto status =
        create_pool(
            device.native_device(),
            &pool_info,
            nullptr,
            &pool);

    if (status != VK_SUCCESS || !pool) {
        diagnostic =
            result_message(
                "vkCreateCommandPool(texture)",
                status);
        return false;
    }

    void* command = nullptr;
    const VkCommandBufferAllocateInfo alloc_info{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        nullptr,
        pool,
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        1
    };

    status =
        allocate_commands(
            device.native_device(),
            &alloc_info,
            &command);

    if (status != VK_SUCCESS || !command) {
        destroy_pool(
            device.native_device(),
            pool,
            nullptr);

        diagnostic =
            result_message(
                "vkAllocateCommandBuffers(texture)",
                status);
        return false;
    }

    const VkCommandBufferBeginInfo begin_info{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        nullptr,
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        nullptr
    };

    status =
        begin_command(
            command,
            &begin_info);

    if (status != VK_SUCCESS) {
        destroy_pool(
            device.native_device(),
            pool,
            nullptr);

        diagnostic =
            result_message(
                "vkBeginCommandBuffer(texture)",
                status);
        return false;
    }

    if (levels.empty()) {
        destroy_pool(
            device.native_device(),
            pool,
            nullptr);

        diagnostic =
            "Vulkan texture upload requires at least one mip level";
        return false;
    }

    const VkImageSubresourceRange range{
        VK_IMAGE_ASPECT_COLOR_BIT,
        0,
        static_cast<std::uint32_t>(
            levels.size()),
        0,
        1
    };

    const VkImageMemoryBarrier to_transfer{
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        nullptr,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        image,
        range
    };

    pipeline_barrier(
        command,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &to_transfer);

    std::vector<VkBufferImageCopy>
        copies;

    copies.reserve(
        levels.size());

    for (std::size_t level = 0u;
         level < levels.size();
         ++level) {

        const auto& mip =
            levels[level];

        copies.push_back({
            mip.offset,
            0,
            0,
            {
                VK_IMAGE_ASPECT_COLOR_BIT,
                static_cast<std::uint32_t>(
                    level),
                0,
                1
            },
            {0, 0, 0},
            {
                mip.width,
                mip.height,
                1
            }
        });
    }

    copy_buffer_to_image(
        command,
        staging_buffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        static_cast<std::uint32_t>(
            copies.size()),
        copies.data());

    const VkImageMemoryBarrier to_shader{
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        nullptr,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        image,
        range
    };

    pipeline_barrier(
        command,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &to_shader);

    status =
        end_command(command);

    if (status != VK_SUCCESS) {
        destroy_pool(
            device.native_device(),
            pool,
            nullptr);

        diagnostic =
            result_message(
                "vkEndCommandBuffer(texture)",
                status);
        return false;
    }

    const VkSubmitInfo submit{
        VK_STRUCTURE_TYPE_SUBMIT_INFO,
        nullptr,
        0,
        nullptr,
        nullptr,
        1,
        &command,
        0,
        nullptr
    };

    status =
        queue_submit(
            device.graphics_queue(),
            1,
            &submit,
            nullptr);

    if (status == VK_SUCCESS) {
        status =
            queue_wait_idle(
                device.graphics_queue());
    }

    destroy_pool(
        device.native_device(),
        pool,
        nullptr);

    if (status != VK_SUCCESS) {
        diagnostic =
            result_message(
                "Vulkan texture upload",
                status);
        return false;
    }

    diagnostic =
        "Vulkan texture upload complete";

    return true;
}

} // namespace

VulkanTextureResource::~VulkanTextureResource() {
    destroy();
}

VulkanTextureResource::VulkanTextureResource(
    VulkanTextureResource&& other) noexcept
    : device_api_(
          std::exchange(
              other.device_api_, nullptr)),
      device_(
          std::exchange(
              other.device_, nullptr)),
      image_(
          std::exchange(
              other.image_, nullptr)),
      memory_(
          std::exchange(
              other.memory_, nullptr)),
      view_(
          std::exchange(
              other.view_, nullptr)),
      sampler_(
          std::exchange(
              other.sampler_, nullptr)),
      width_(
          std::exchange(
              other.width_, 0)),
      height_(
          std::exchange(
              other.height_, 0)),
      format_(
          std::exchange(
              other.format_, 0)),
      mip_levels_(
          std::exchange(
              other.mip_levels_, 0)),
      diagnostic_(
          std::move(
              other.diagnostic_)) {}

VulkanTextureResource&
VulkanTextureResource::operator=(
    VulkanTextureResource&& other) noexcept {

    if (this == &other) {
        return *this;
    }

    destroy();

    device_api_ =
        std::exchange(
            other.device_api_, nullptr);
    device_ =
        std::exchange(
            other.device_, nullptr);
    image_ =
        std::exchange(
            other.image_, nullptr);
    memory_ =
        std::exchange(
            other.memory_, nullptr);
    view_ =
        std::exchange(
            other.view_, nullptr);
    sampler_ =
        std::exchange(
            other.sampler_, nullptr);
    width_ =
        std::exchange(
            other.width_, 0);
    height_ =
        std::exchange(
            other.height_, 0);
    format_ =
        std::exchange(
            other.format_, 0);
    mip_levels_ =
        std::exchange(
            other.mip_levels_, 0);
    diagnostic_ =
        std::move(
            other.diagnostic_);

    return *this;
}

bool VulkanTextureResource::create_rgba8(
    const VulkanLoader& loader,
    const VulkanInstance& instance,
    const VulkanDevice& device,
    std::uint32_t width,
    std::uint32_t height,
    const void* pixels,
    std::size_t pixel_bytes,
    VulkanTextureColorSpace color_space) {

    destroy();
    diagnostic_.clear();

    if (!loader.loaded() ||
        !instance.valid() ||
        !device.valid()) {

        diagnostic_ =
            "valid Vulkan runtime is required";
        return false;
    }

    Rgba8MipChain mip_chain;

    if (!build_rgba8_mip_chain(
            width,
            height,
            pixels,
            pixel_bytes,
            color_space,
            mip_chain,
            diagnostic_)) {

        return false;
    }

    VulkanBufferResource staging;

    if (!staging.create(
            loader,
            instance,
            device,
            mip_chain.bytes.size(),
            VulkanBufferUsage::TransferSource,
            VulkanMemoryPreference::HostVisible,
            mip_chain.bytes.data())) {

        diagnostic_ =
            "texture staging buffer failed: " +
            staging.diagnostic();
        return false;
    }

    const auto create_image =
        load_device_proc<CreateImage>(
            device, "vkCreateImage");
    const auto destroy_image =
        load_device_proc<DestroyImage>(
            device, "vkDestroyImage");
    const auto get_requirements =
        load_device_proc<GetImageMemoryRequirements>(
            device, "vkGetImageMemoryRequirements");
    const auto allocate_memory =
        load_device_proc<AllocateMemory>(
            device, "vkAllocateMemory");
    const auto free_memory =
        load_device_proc<FreeMemory>(
            device, "vkFreeMemory");
    const auto bind_memory =
        load_device_proc<BindImageMemory>(
            device, "vkBindImageMemory");
    const auto create_view =
        load_device_proc<CreateImageView>(
            device, "vkCreateImageView");
    const auto create_sampler =
        load_device_proc<CreateSampler>(
            device, "vkCreateSampler");

    const auto memory_properties =
        reinterpret_cast<
            GetPhysicalDeviceMemoryProperties>(
                loader.get_instance_proc_address(
                    instance.native_handle(),
                    "vkGetPhysicalDeviceMemoryProperties"));

    if (!create_image || !destroy_image ||
        !get_requirements || !allocate_memory ||
        !free_memory || !bind_memory ||
        !create_view || !create_sampler ||
        !memory_properties) {

        diagnostic_ =
            "required Vulkan texture functions are unavailable";
        return false;
    }

    const std::uint32_t format =
        color_space ==
            VulkanTextureColorSpace::SRgb
            ? VK_FORMAT_R8G8B8A8_SRGB
            : VK_FORMAT_R8G8B8A8_UNORM;

    const VkImageCreateInfo image_info{
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        nullptr,
        0,
        VK_IMAGE_TYPE_2D,
        format,
        {width, height, 1},
        static_cast<std::uint32_t>(
            mip_chain.levels.size()),
        1,
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT |
            VK_IMAGE_USAGE_SAMPLED_BIT,
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

    if (status != VK_SUCCESS || !image) {
        diagnostic_ =
            result_message(
                "vkCreateImage(texture)",
                status);
        return false;
    }

    VkMemoryRequirements requirements{};
    get_requirements(
        device.native_device(),
        image,
        &requirements);

    VkPhysicalDeviceMemoryProperties props{};
    memory_properties(
        device.physical_device(),
        &props);

    const auto memory_type =
        find_memory_type(
            props,
            requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (!memory_type) {
        destroy_image(
            device.native_device(),
            image,
            nullptr);
        diagnostic_ =
            "no device-local memory type for Vulkan texture";
        return false;
    }

    const VkMemoryAllocateInfo alloc_info{
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        nullptr,
        requirements.size,
        *memory_type
    };

    void* memory = nullptr;
    status =
        allocate_memory(
            device.native_device(),
            &alloc_info,
            nullptr,
            &memory);

    if (status != VK_SUCCESS || !memory) {
        destroy_image(
            device.native_device(),
            image,
            nullptr);
        diagnostic_ =
            result_message(
                "vkAllocateMemory(texture)",
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
        destroy_image(
            device.native_device(),
            image,
            nullptr);
        free_memory(
            device.native_device(),
            memory,
            nullptr);
        diagnostic_ =
            result_message(
                "vkBindImageMemory(texture)",
                status);
        return false;
    }

    if (!upload_image_sync(
            device,
            staging.native_buffer(),
            image,
            mip_chain.levels,
            diagnostic_)) {

        destroy_image(
            device.native_device(),
            image,
            nullptr);
        free_memory(
            device.native_device(),
            memory,
            nullptr);
        return false;
    }

    const VkImageViewCreateInfo view_info{
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        nullptr,
        0,
        image,
        VK_IMAGE_VIEW_TYPE_2D,
        format,
        {
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY
        },
        {
            VK_IMAGE_ASPECT_COLOR_BIT,
            0,
            static_cast<std::uint32_t>(
                mip_chain.levels.size()),
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

    if (status != VK_SUCCESS || !view) {
        destroy_image(
            device.native_device(),
            image,
            nullptr);
        free_memory(
            device.native_device(),
            memory,
            nullptr);
        diagnostic_ =
            result_message(
                "vkCreateImageView(texture)",
                status);
        return false;
    }

    const VkSamplerCreateInfo sampler_info{
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        nullptr,
        0,
        VK_FILTER_LINEAR,
        VK_FILTER_LINEAR,
        VK_SAMPLER_MIPMAP_MODE_LINEAR,
        VK_SAMPLER_ADDRESS_MODE_REPEAT,
        VK_SAMPLER_ADDRESS_MODE_REPEAT,
        VK_SAMPLER_ADDRESS_MODE_REPEAT,
        0.0f,
        0,
        1.0f,
        0,
        VK_COMPARE_OP_ALWAYS,
        0.0f,
        static_cast<float>(
            mip_chain.levels.size() -
            1u),
        VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        0
    };

    void* sampler = nullptr;
    status =
        create_sampler(
            device.native_device(),
            &sampler_info,
            nullptr,
            &sampler);

    if (status != VK_SUCCESS || !sampler) {
        const auto destroy_view =
            load_device_proc<DestroyImageView>(
                device, "vkDestroyImageView");

        if (destroy_view) {
            destroy_view(
                device.native_device(),
                view,
                nullptr);
        }

        destroy_image(
            device.native_device(),
            image,
            nullptr);
        free_memory(
            device.native_device(),
            memory,
            nullptr);

        diagnostic_ =
            result_message(
                "vkCreateSampler",
                status);
        return false;
    }

    device_api_ = &device;
    device_ =
        device.native_device();
    image_ = image;
    memory_ = memory;
    view_ = view;
    sampler_ = sampler;
    width_ = width;
    height_ = height;
    format_ = format;
    mip_levels_ =
        static_cast<std::uint32_t>(
            mip_chain.levels.size());

    diagnostic_ =
        "Vulkan RGBA8 texture image/view/sampler created with " +
        std::to_string(
            mip_levels_) +
        " mip level(s)";

    return true;
}

void VulkanTextureResource::destroy() noexcept {
    if (device_api_ && device_) {
        const auto destroy_sampler =
            load_device_proc<DestroySampler>(
                *device_api_,
                "vkDestroySampler");
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

        if (destroy_sampler && sampler_) {
            destroy_sampler(
                device_,
                sampler_,
                nullptr);
        }

        if (destroy_view && view_) {
            destroy_view(
                device_,
                view_,
                nullptr);
        }

        if (destroy_image && image_) {
            destroy_image(
                device_,
                image_,
                nullptr);
        }

        if (free_memory && memory_) {
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
    sampler_ = nullptr;
    width_ = 0;
    height_ = 0;
    format_ = 0;
    mip_levels_ = 0;
}

} // namespace nengine::render
