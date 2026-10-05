#include "nengine/render/vulkan_buffer.hpp"

#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
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
VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO = 12;

constexpr std::uint32_t
VK_SHARING_MODE_EXCLUSIVE = 0;

constexpr std::uint32_t
VK_BUFFER_USAGE_TRANSFER_SRC_BIT = 0x00000001u;

constexpr std::uint32_t
VK_BUFFER_USAGE_TRANSFER_DST_BIT = 0x00000002u;

constexpr std::uint32_t
VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT = 0x00000010u;

constexpr std::uint32_t
VK_BUFFER_USAGE_STORAGE_BUFFER_BIT = 0x00000020u;

constexpr std::uint32_t
VK_BUFFER_USAGE_INDEX_BUFFER_BIT = 0x00000040u;

constexpr std::uint32_t
VK_BUFFER_USAGE_VERTEX_BUFFER_BIT = 0x00000080u;

constexpr std::uint32_t
VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT = 0x00000001u;

constexpr std::uint32_t
VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT = 0x00000002u;

constexpr std::uint32_t
VK_MEMORY_PROPERTY_HOST_COHERENT_BIT = 0x00000004u;

constexpr std::uint32_t
VK_MAX_MEMORY_TYPES = 32;

constexpr std::uint32_t
VK_MAX_MEMORY_HEAPS = 16;

struct VkBufferCreateInfo {
    std::uint32_t sType;
    const void* pNext;
    std::uint32_t flags;
    VkDeviceSize size;
    std::uint32_t usage;
    std::uint32_t sharingMode;
    std::uint32_t queueFamilyIndexCount;
    const std::uint32_t* pQueueFamilyIndices;
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

using CreateBuffer =
    VkResult (*)(
        void*,
        const VkBufferCreateInfo*,
        const void*,
        void**);

using DestroyBuffer =
    void (*)(
        void*,
        void*,
        const void*);

using GetBufferMemoryRequirements =
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

using BindBufferMemory =
    VkResult (*)(
        void*,
        void*,
        void*,
        VkDeviceSize);

using MapMemory =
    VkResult (*)(
        void*,
        void*,
        VkDeviceSize,
        VkDeviceSize,
        std::uint32_t,
        void**);

using UnmapMemory =
    void (*)(
        void*,
        void*);


constexpr std::uint32_t
VK_STRUCTURE_TYPE_SUBMIT_INFO = 4;

constexpr std::uint32_t
VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO = 39;

constexpr std::uint32_t
VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO = 40;

constexpr std::uint32_t
VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO = 42;

constexpr std::uint32_t
VK_COMMAND_POOL_CREATE_TRANSIENT_BIT = 0x00000001u;

constexpr std::uint32_t
VK_COMMAND_BUFFER_LEVEL_PRIMARY = 0u;

constexpr std::uint32_t
VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT = 0x00000001u;

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

struct VkBufferCopy {
    VkDeviceSize srcOffset;
    VkDeviceSize dstOffset;
    VkDeviceSize size;
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

using CreateCommandPool =
    VkResult (*)(
        void*,
        const VkCommandPoolCreateInfo*,
        const void*,
        void**);

using DestroyCommandPool =
    void (*)(
        void*,
        void*,
        const void*);

using AllocateCommandBuffers =
    VkResult (*)(
        void*,
        const VkCommandBufferAllocateInfo*,
        void**);

using BeginCommandBuffer =
    VkResult (*)(
        void*,
        const VkCommandBufferBeginInfo*);

using EndCommandBuffer =
    VkResult (*)(void*);

using CmdCopyBuffer =
    void (*)(
        void*,
        void*,
        void*,
        std::uint32_t,
        const VkBufferCopy*);

using QueueSubmit =
    VkResult (*)(
        void*,
        std::uint32_t,
        const VkSubmitInfo*,
        void*);

using QueueWaitIdle =
    VkResult (*)(void*);

std::string result_message(
    std::string_view operation,
    VkResult result) {

    return
        std::string{operation} +
        " failed with VkResult " +
        std::to_string(result);
}

std::uint32_t translate_usage(
    VulkanBufferUsage usage) {

    const auto raw =
        static_cast<std::uint32_t>(
            usage);

    std::uint32_t result = 0;

    if ((raw &
         static_cast<std::uint32_t>(
             VulkanBufferUsage::Vertex)) != 0) {
        result |=
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    }

    if ((raw &
         static_cast<std::uint32_t>(
             VulkanBufferUsage::Index)) != 0) {
        result |=
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    }

    if ((raw &
         static_cast<std::uint32_t>(
             VulkanBufferUsage::Uniform)) != 0) {
        result |=
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    }

    if ((raw &
         static_cast<std::uint32_t>(
             VulkanBufferUsage::Storage)) != 0) {
        result |=
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    }

    if ((raw &
         static_cast<std::uint32_t>(
             VulkanBufferUsage::TransferSource)) != 0) {
        result |=
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    }

    if ((raw &
         static_cast<std::uint32_t>(
             VulkanBufferUsage::TransferDestination)) != 0) {
        result |=
            VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }

    return result;
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

template <typename T>
T load_device_proc(
    const VulkanDevice& device,
    const char* name) {

    return reinterpret_cast<T>(
        device.get_proc_address(name));
}

bool copy_buffer_sync(
    const VulkanDevice& device,
    void* source,
    void* destination,
    std::size_t size_bytes,
    std::string& diagnostic) {

    const auto create_pool =
        load_device_proc<CreateCommandPool>(
            device,
            "vkCreateCommandPool");

    const auto destroy_pool =
        load_device_proc<DestroyCommandPool>(
            device,
            "vkDestroyCommandPool");

    const auto allocate_commands =
        load_device_proc<AllocateCommandBuffers>(
            device,
            "vkAllocateCommandBuffers");

    const auto begin_command =
        load_device_proc<BeginCommandBuffer>(
            device,
            "vkBeginCommandBuffer");

    const auto end_command =
        load_device_proc<EndCommandBuffer>(
            device,
            "vkEndCommandBuffer");

    const auto copy_buffer =
        load_device_proc<CmdCopyBuffer>(
            device,
            "vkCmdCopyBuffer");

    const auto queue_submit =
        load_device_proc<QueueSubmit>(
            device,
            "vkQueueSubmit");

    const auto queue_wait_idle =
        load_device_proc<QueueWaitIdle>(
            device,
            "vkQueueWaitIdle");

    if (!create_pool ||
        !destroy_pool ||
        !allocate_commands ||
        !begin_command ||
        !end_command ||
        !copy_buffer ||
        !queue_submit ||
        !queue_wait_idle) {

        diagnostic =
            "required Vulkan staging-copy functions are unavailable";
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

    if (status != VK_SUCCESS ||
        !pool) {

        diagnostic =
            result_message(
                "vkCreateCommandPool",
                status);
        return false;
    }

    void* command = nullptr;

    const VkCommandBufferAllocateInfo allocate_info{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        nullptr,
        pool,
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        1
    };

    status =
        allocate_commands(
            device.native_device(),
            &allocate_info,
            &command);

    if (status != VK_SUCCESS ||
        !command) {

        destroy_pool(
            device.native_device(),
            pool,
            nullptr);

        diagnostic =
            result_message(
                "vkAllocateCommandBuffers",
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
                "vkBeginCommandBuffer",
                status);
        return false;
    }

    const VkBufferCopy region{
        0,
        0,
        static_cast<VkDeviceSize>(
            size_bytes)
    };

    copy_buffer(
        command,
        source,
        destination,
        1,
        &region);

    status =
        end_command(command);

    if (status != VK_SUCCESS) {
        destroy_pool(
            device.native_device(),
            pool,
            nullptr);

        diagnostic =
            result_message(
                "vkEndCommandBuffer",
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
                "Vulkan staging buffer copy",
                status);
        return false;
    }

    diagnostic =
        "Vulkan staging buffer copy complete";

    return true;
}

} // namespace

VulkanBufferResource::~VulkanBufferResource() {
    destroy();
}

VulkanBufferResource::VulkanBufferResource(
    VulkanBufferResource&& other) noexcept
    : device_api_(
          std::exchange(
              other.device_api_,
              nullptr)),
      device_(
          std::exchange(
              other.device_,
              nullptr)),
      buffer_(
          std::exchange(
              other.buffer_,
              nullptr)),
      memory_(
          std::exchange(
              other.memory_,
              nullptr)),
      size_bytes_(
          std::exchange(
              other.size_bytes_,
              0)),
      host_visible_(
          std::exchange(
              other.host_visible_,
              false)),
      diagnostic_(
          std::move(
              other.diagnostic_)) {}

VulkanBufferResource&
VulkanBufferResource::operator=(
    VulkanBufferResource&& other) noexcept {

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

    buffer_ =
        std::exchange(
            other.buffer_,
            nullptr);

    memory_ =
        std::exchange(
            other.memory_,
            nullptr);

    size_bytes_ =
        std::exchange(
            other.size_bytes_,
            0);

    host_visible_ =
        std::exchange(
            other.host_visible_,
            false);

    diagnostic_ =
        std::move(
            other.diagnostic_);

    return *this;
}

bool VulkanBufferResource::create(
    const VulkanLoader& loader,
    const VulkanInstance& instance,
    const VulkanDevice& device,
    std::size_t size_bytes,
    VulkanBufferUsage usage,
    VulkanMemoryPreference memory,
    const void* initial_data) {

    destroy();
    diagnostic_.clear();

    if (!loader.loaded() ||
        !instance.valid() ||
        !device.valid() ||
        size_bytes == 0) {

        diagnostic_ =
            "valid Vulkan instance device and non-zero buffer size are required";
        return false;
    }

    const auto create_buffer =
        load_device_proc<CreateBuffer>(
            device,
            "vkCreateBuffer");

    const auto destroy_buffer =
        load_device_proc<DestroyBuffer>(
            device,
            "vkDestroyBuffer");

    const auto get_requirements =
        load_device_proc<
            GetBufferMemoryRequirements>(
                device,
                "vkGetBufferMemoryRequirements");

    const auto allocate_memory =
        load_device_proc<AllocateMemory>(
            device,
            "vkAllocateMemory");

    const auto free_memory =
        load_device_proc<FreeMemory>(
            device,
            "vkFreeMemory");

    const auto bind_memory =
        load_device_proc<BindBufferMemory>(
            device,
            "vkBindBufferMemory");

    const auto get_memory_properties =
        reinterpret_cast<
            GetPhysicalDeviceMemoryProperties>(
                loader.get_instance_proc_address(
                    instance.native_handle(),
                    "vkGetPhysicalDeviceMemoryProperties"));

    if (!create_buffer ||
        !destroy_buffer ||
        !get_requirements ||
        !allocate_memory ||
        !free_memory ||
        !bind_memory ||
        !get_memory_properties) {

        diagnostic_ =
            "required Vulkan buffer/memory functions are unavailable";
        return false;
    }

    auto native_usage =
        translate_usage(usage);

    if (initial_data &&
        memory ==
            VulkanMemoryPreference::DeviceLocal) {

        native_usage |=
            VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }

    if (native_usage == 0) {
        diagnostic_ =
            "Vulkan buffer usage is empty";
        return false;
    }

    const VkBufferCreateInfo info{
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        nullptr,
        0,
        static_cast<VkDeviceSize>(
            size_bytes),
        native_usage,
        VK_SHARING_MODE_EXCLUSIVE,
        0,
        nullptr
    };

    void* buffer = nullptr;

    auto status =
        create_buffer(
            device.native_device(),
            &info,
            nullptr,
            &buffer);

    if (status != VK_SUCCESS ||
        !buffer) {

        diagnostic_ =
            result_message(
                "vkCreateBuffer",
                status);
        return false;
    }

    VkMemoryRequirements requirements{};

    get_requirements(
        device.native_device(),
        buffer,
        &requirements);

    VkPhysicalDeviceMemoryProperties
        memory_properties{};

    get_memory_properties(
        device.physical_device(),
        &memory_properties);

    std::uint32_t required_flags =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    if (memory ==
        VulkanMemoryPreference::HostVisible) {

        required_flags =
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    }

    const auto memory_type =
        find_memory_type(
            memory_properties,
            requirements.memoryTypeBits,
            required_flags);

    if (!memory_type) {
        destroy_buffer(
            device.native_device(),
            buffer,
            nullptr);

        diagnostic_ =
            "no compatible Vulkan memory type found";
        return false;
    }

    const VkMemoryAllocateInfo allocate_info{
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        nullptr,
        requirements.size,
        *memory_type
    };

    void* allocation = nullptr;

    status =
        allocate_memory(
            device.native_device(),
            &allocate_info,
            nullptr,
            &allocation);

    if (status != VK_SUCCESS ||
        !allocation) {

        destroy_buffer(
            device.native_device(),
            buffer,
            nullptr);

        diagnostic_ =
            result_message(
                "vkAllocateMemory",
                status);

        return false;
    }

    status =
        bind_memory(
            device.native_device(),
            buffer,
            allocation,
            0);

    if (status != VK_SUCCESS) {
        free_memory(
            device.native_device(),
            allocation,
            nullptr);

        destroy_buffer(
            device.native_device(),
            buffer,
            nullptr);

        diagnostic_ =
            result_message(
                "vkBindBufferMemory",
                status);

        return false;
    }

    device_api_ = &device;
    device_ =
        device.native_device();
    buffer_ = buffer;
    memory_ = allocation;
    size_bytes_ = size_bytes;
    host_visible_ =
        memory ==
        VulkanMemoryPreference::HostVisible;

    if (initial_data) {
        if (host_visible_) {
            if (!upload(
                    initial_data,
                    size_bytes,
                    0)) {

                const auto upload_error =
                    diagnostic_;

                destroy();

                diagnostic_ =
                    upload_error;

                return false;
            }
        } else {
            VulkanBufferResource staging;

            if (!staging.create(
                    loader,
                    instance,
                    device,
                    size_bytes,
                    VulkanBufferUsage::TransferSource,
                    VulkanMemoryPreference::HostVisible,
                    initial_data)) {

                const auto staging_error =
                    staging.diagnostic();

                destroy();

                diagnostic_ =
                    "device-local staging allocation failed: " +
                    staging_error;

                return false;
            }

            if (!copy_buffer_sync(
                    device,
                    staging.native_buffer(),
                    buffer_,
                    size_bytes,
                    diagnostic_)) {

                const auto copy_error =
                    diagnostic_;

                destroy();

                diagnostic_ =
                    copy_error;

                return false;
            }
        }
    }

    diagnostic_ =
        host_visible_
            ? "host-visible Vulkan buffer created"
            : "device-local Vulkan buffer created";

    return true;
}

bool VulkanBufferResource::upload(
    const void* data,
    std::size_t size_bytes,
    std::size_t offset) {

    if (!valid() ||
        !host_visible_ ||
        !data ||
        size_bytes == 0 ||
        offset > size_bytes_ ||
        size_bytes >
            size_bytes_ - offset) {

        diagnostic_ =
            "host-visible Vulkan buffer and in-range upload are required";
        return false;
    }

    const auto map_memory =
        load_device_proc<MapMemory>(
            *device_api_,
            "vkMapMemory");

    const auto unmap_memory =
        load_device_proc<UnmapMemory>(
            *device_api_,
            "vkUnmapMemory");

    if (!map_memory ||
        !unmap_memory) {

        diagnostic_ =
            "Vulkan memory map functions are unavailable";
        return false;
    }

    void* mapped = nullptr;

    const auto status =
        map_memory(
            device_,
            memory_,
            static_cast<VkDeviceSize>(
                offset),
            static_cast<VkDeviceSize>(
                size_bytes),
            0,
            &mapped);

    if (status != VK_SUCCESS ||
        !mapped) {

        diagnostic_ =
            result_message(
                "vkMapMemory",
                status);

        return false;
    }

    std::memcpy(
        mapped,
        data,
        size_bytes);

    unmap_memory(
        device_,
        memory_);

    diagnostic_ =
        "Vulkan buffer upload complete";

    return true;
}

void VulkanBufferResource::destroy() noexcept {
    if (device_api_ &&
        device_) {

        const auto destroy_buffer =
            load_device_proc<DestroyBuffer>(
                *device_api_,
                "vkDestroyBuffer");

        const auto free_memory =
            load_device_proc<FreeMemory>(
                *device_api_,
                "vkFreeMemory");

        if (destroy_buffer &&
            buffer_) {

            destroy_buffer(
                device_,
                buffer_,
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
    buffer_ = nullptr;
    memory_ = nullptr;
    size_bytes_ = 0;
    host_visible_ = false;
}

} // namespace nengine::render
