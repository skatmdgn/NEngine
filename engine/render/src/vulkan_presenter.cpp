#include "nengine/render/vulkan_presenter.hpp"

#include <cstdint>
#include <limits>
#include <string_view>
#include <vector>

namespace nengine::render {
namespace {

using VkResult = std::int32_t;

constexpr VkResult VK_SUCCESS = 0;
constexpr VkResult VK_SUBOPTIMAL_KHR = 1000001003;
constexpr VkResult VK_ERROR_OUT_OF_DATE_KHR = -1000001004;

constexpr std::uint32_t
VK_STRUCTURE_TYPE_SUBMIT_INFO = 4;

constexpr std::uint32_t
VK_STRUCTURE_TYPE_FENCE_CREATE_INFO = 8;

constexpr std::uint32_t
VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO = 9;

constexpr std::uint32_t
VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO = 39;

constexpr std::uint32_t
VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO = 40;

constexpr std::uint32_t
VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO = 42;

constexpr std::uint32_t
VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER = 45;

constexpr std::uint32_t
VK_STRUCTURE_TYPE_PRESENT_INFO_KHR = 1000001001;

constexpr std::uint32_t
VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT =
    0x00000002u;

constexpr std::uint32_t
VK_FENCE_CREATE_SIGNALED_BIT =
    0x00000001u;

constexpr std::uint32_t
VK_COMMAND_BUFFER_LEVEL_PRIMARY = 0u;

constexpr std::uint32_t
VK_IMAGE_LAYOUT_UNDEFINED = 0u;

constexpr std::uint32_t
VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL = 7u;

constexpr std::uint32_t
VK_IMAGE_LAYOUT_PRESENT_SRC_KHR = 1000001002u;

constexpr std::uint32_t
VK_ACCESS_TRANSFER_WRITE_BIT = 0x00001000u;

constexpr std::uint32_t
VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT = 0x00000001u;

constexpr std::uint32_t
VK_PIPELINE_STAGE_TRANSFER_BIT = 0x00001000u;

constexpr std::uint32_t
VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT = 0x00002000u;

constexpr std::uint32_t
VK_IMAGE_ASPECT_COLOR_BIT = 0x00000001u;

constexpr std::uint32_t
VK_QUEUE_FAMILY_IGNORED = 0xFFFFFFFFu;

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

struct VkSemaphoreCreateInfo {
    std::uint32_t sType;
    const void* pNext;
    std::uint32_t flags;
};

struct VkFenceCreateInfo {
    std::uint32_t sType;
    const void* pNext;
    std::uint32_t flags;
};

struct VkImageSubresourceRange {
    std::uint32_t aspectMask;
    std::uint32_t baseMipLevel;
    std::uint32_t levelCount;
    std::uint32_t baseArrayLayer;
    std::uint32_t layerCount;
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

union VkClearColorValue {
    float float32[4];
    std::int32_t int32[4];
    std::uint32_t uint32[4];
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

struct VkPresentInfoKHR {
    std::uint32_t sType;
    const void* pNext;
    std::uint32_t waitSemaphoreCount;
    void* const* pWaitSemaphores;
    std::uint32_t swapchainCount;
    void* const* pSwapchains;
    const std::uint32_t* pImageIndices;
    VkResult* pResults;
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

using CreateSemaphore =
    VkResult (*)(
        void*,
        const VkSemaphoreCreateInfo*,
        const void*,
        void**);

using DestroySemaphore =
    void (*)(
        void*,
        void*,
        const void*);

using CreateFence =
    VkResult (*)(
        void*,
        const VkFenceCreateInfo*,
        const void*,
        void**);

using DestroyFence =
    void (*)(
        void*,
        void*,
        const void*);

using WaitForFences =
    VkResult (*)(
        void*,
        std::uint32_t,
        void* const*,
        std::uint32_t,
        std::uint64_t);

using ResetFences =
    VkResult (*)(
        void*,
        std::uint32_t,
        void* const*);

using ResetCommandBuffer =
    VkResult (*)(
        void*,
        std::uint32_t);

using BeginCommandBuffer =
    VkResult (*)(
        void*,
        const VkCommandBufferBeginInfo*);

using EndCommandBuffer =
    VkResult (*)(
        void*);

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

using CmdClearColorImage =
    void (*)(
        void*,
        void*,
        std::uint32_t,
        const VkClearColorValue*,
        std::uint32_t,
        const VkImageSubresourceRange*);

using AcquireNextImage =
    VkResult (*)(
        void*,
        void*,
        std::uint64_t,
        void*,
        void*,
        std::uint32_t*);

using QueueSubmit =
    VkResult (*)(
        void*,
        std::uint32_t,
        const VkSubmitInfo*,
        void*);

using QueuePresent =
    VkResult (*)(
        void*,
        const VkPresentInfoKHR*);

using DeviceWaitIdle =
    VkResult (*)(void*);

std::string result_message(
    std::string_view operation,
    VkResult result) {

    return
        std::string{operation} +
        " failed with VkResult " +
        std::to_string(result);
}

template <typename T>
T load_proc(
    const VulkanDevice& device,
    const char* name) {

    return reinterpret_cast<T>(
        device.get_proc_address(name));
}

} // namespace

VulkanClearPresenter::~VulkanClearPresenter() {
    shutdown();
}

bool VulkanClearPresenter::initialize(
    const VulkanDevice& device,
    const VulkanSwapchain& swapchain) {

    shutdown();
    diagnostic_.clear();
    needs_resize_ = false;

    if (!device.valid() ||
        !swapchain.valid()) {

        diagnostic_ =
            "Vulkan device and swapchain are required";
        return false;
    }

    const auto create_pool =
        load_proc<CreateCommandPool>(
            device,
            "vkCreateCommandPool");

    const auto allocate_commands =
        load_proc<AllocateCommandBuffers>(
            device,
            "vkAllocateCommandBuffers");

    const auto create_semaphore =
        load_proc<CreateSemaphore>(
            device,
            "vkCreateSemaphore");

    const auto create_fence =
        load_proc<CreateFence>(
            device,
            "vkCreateFence");

    if (!create_pool ||
        !allocate_commands ||
        !create_semaphore ||
        !create_fence) {

        diagnostic_ =
            "required Vulkan frame-resource creation functions are unavailable";
        return false;
    }

    const VkCommandPoolCreateInfo pool_info{
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        nullptr,
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
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

        diagnostic_ =
            result_message(
                "vkCreateCommandPool",
                status);
        return false;
    }

    std::vector<void*> command_buffers(
        swapchain.images().size());

    const VkCommandBufferAllocateInfo allocate_info{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        nullptr,
        pool,
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        static_cast<std::uint32_t>(
            command_buffers.size())
    };

    status =
        allocate_commands(
            device.native_device(),
            &allocate_info,
            command_buffers.data());

    if (status != VK_SUCCESS) {
        const auto destroy_pool =
            load_proc<DestroyCommandPool>(
                device,
                "vkDestroyCommandPool");

        if (destroy_pool) {
            destroy_pool(
                device.native_device(),
                pool,
                nullptr);
        }

        diagnostic_ =
            result_message(
                "vkAllocateCommandBuffers",
                status);
        return false;
    }

    const VkSemaphoreCreateInfo semaphore_info{
        VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        nullptr,
        0
    };

    void* image_available = nullptr;
    void* render_finished = nullptr;

    status =
        create_semaphore(
            device.native_device(),
            &semaphore_info,
            nullptr,
            &image_available);

    if (status == VK_SUCCESS) {
        status =
            create_semaphore(
                device.native_device(),
                &semaphore_info,
                nullptr,
                &render_finished);
    }

    const VkFenceCreateInfo fence_info{
        VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        nullptr,
        VK_FENCE_CREATE_SIGNALED_BIT
    };

    void* frame_fence = nullptr;

    if (status == VK_SUCCESS) {
        status =
            create_fence(
                device.native_device(),
                &fence_info,
                nullptr,
                &frame_fence);
    }

    if (status != VK_SUCCESS ||
        !image_available ||
        !render_finished ||
        !frame_fence) {

        const auto destroy_semaphore =
            load_proc<DestroySemaphore>(
                device,
                "vkDestroySemaphore");

        const auto destroy_fence =
            load_proc<DestroyFence>(
                device,
                "vkDestroyFence");

        const auto destroy_pool =
            load_proc<DestroyCommandPool>(
                device,
                "vkDestroyCommandPool");

        if (destroy_fence &&
            frame_fence) {
            destroy_fence(
                device.native_device(),
                frame_fence,
                nullptr);
        }

        if (destroy_semaphore &&
            render_finished) {
            destroy_semaphore(
                device.native_device(),
                render_finished,
                nullptr);
        }

        if (destroy_semaphore &&
            image_available) {
            destroy_semaphore(
                device.native_device(),
                image_available,
                nullptr);
        }

        if (destroy_pool) {
            destroy_pool(
                device.native_device(),
                pool,
                nullptr);
        }

        diagnostic_ =
            result_message(
                "Vulkan frame synchronization creation",
                status);

        return false;
    }

    device_api_ = &device;
    device_ =
        device.native_device();
    queue_ =
        device.graphics_queue();
    swapchain_ =
        swapchain.native_handle();
    images_ =
        swapchain.images();
    command_buffers_ =
        std::move(command_buffers);
    image_initialized_.assign(
        images_.size(),
        false);
    command_pool_ = pool;
    image_available_ =
        image_available;
    render_finished_ =
        render_finished;
    frame_fence_ =
        frame_fence;

    diagnostic_ =
        "Vulkan clear presenter ready";

    return true;
}

bool VulkanClearPresenter::present_clear(
    float red,
    float green,
    float blue,
    float alpha) {

    if (!ready()) {
        diagnostic_ =
            "Vulkan clear presenter is not initialized";
        return false;
    }

    needs_resize_ = false;

    const auto wait_fences =
        load_proc<WaitForFences>(
            *device_api_,
            "vkWaitForFences");

    const auto reset_fences =
        load_proc<ResetFences>(
            *device_api_,
            "vkResetFences");

    const auto acquire =
        load_proc<AcquireNextImage>(
            *device_api_,
            "vkAcquireNextImageKHR");

    const auto reset_command =
        load_proc<ResetCommandBuffer>(
            *device_api_,
            "vkResetCommandBuffer");

    const auto begin_command =
        load_proc<BeginCommandBuffer>(
            *device_api_,
            "vkBeginCommandBuffer");

    const auto end_command =
        load_proc<EndCommandBuffer>(
            *device_api_,
            "vkEndCommandBuffer");

    const auto barrier =
        load_proc<CmdPipelineBarrier>(
            *device_api_,
            "vkCmdPipelineBarrier");

    const auto clear_image =
        load_proc<CmdClearColorImage>(
            *device_api_,
            "vkCmdClearColorImage");

    const auto queue_submit =
        load_proc<QueueSubmit>(
            *device_api_,
            "vkQueueSubmit");

    const auto queue_present =
        load_proc<QueuePresent>(
            *device_api_,
            "vkQueuePresentKHR");

    if (!wait_fences ||
        !reset_fences ||
        !acquire ||
        !reset_command ||
        !begin_command ||
        !end_command ||
        !barrier ||
        !clear_image ||
        !queue_submit ||
        !queue_present) {

        diagnostic_ =
            "required Vulkan frame functions are unavailable";
        return false;
    }

    void* fence =
        frame_fence_;

    auto status =
        wait_fences(
            device_,
            1,
            &fence,
            1,
            std::numeric_limits<
                std::uint64_t>::max());

    if (status != VK_SUCCESS) {
        diagnostic_ =
            result_message(
                "vkWaitForFences",
                status);
        return false;
    }

    std::uint32_t image_index = 0;

    status =
        acquire(
            device_,
            swapchain_,
            std::numeric_limits<
                std::uint64_t>::max(),
            image_available_,
            nullptr,
            &image_index);

    if (status ==
            VK_ERROR_OUT_OF_DATE_KHR) {

        needs_resize_ = true;
        diagnostic_ =
            "Vulkan swapchain is out of date";
        return false;
    }

    if (status != VK_SUCCESS &&
        status != VK_SUBOPTIMAL_KHR) {

        diagnostic_ =
            result_message(
                "vkAcquireNextImageKHR",
                status);
        return false;
    }

    if (status ==
        VK_SUBOPTIMAL_KHR) {
        needs_resize_ = true;
    }

    if (image_index >=
        command_buffers_.size()) {

        diagnostic_ =
            "Vulkan acquired image index is outside command buffer range";
        return false;
    }

    status =
        reset_fences(
            device_,
            1,
            &fence);

    if (status != VK_SUCCESS) {
        diagnostic_ =
            result_message(
                "vkResetFences",
                status);
        return false;
    }

    auto fail_after_fence_reset =
        [this](
            std::string message) {

            shutdown();
            diagnostic_ =
                std::move(message);
            needs_resize_ = true;
            return false;
        };

    auto* command =
        command_buffers_[image_index];

    status =
        reset_command(
            command,
            0);

    if (status != VK_SUCCESS) {
        return fail_after_fence_reset(
            result_message(
                "vkResetCommandBuffer",
                status));
    }

    const VkCommandBufferBeginInfo
        begin_info{
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            nullptr,
            0,
            nullptr
        };

    status =
        begin_command(
            command,
            &begin_info);

    if (status != VK_SUCCESS) {
        return fail_after_fence_reset(
            result_message(
                "vkBeginCommandBuffer",
                status));
    }

    const VkImageSubresourceRange range{
        VK_IMAGE_ASPECT_COLOR_BIT,
        0,
        1,
        0,
        1
    };

    const bool initialized =
        image_initialized_[
            image_index];

    const VkImageMemoryBarrier
        to_transfer{
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            nullptr,
            0,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            initialized
                ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
                : VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            images_[image_index],
            range
        };

    barrier(
        command,
        initialized
            ? VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
            : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &to_transfer);

    const VkClearColorValue color{{
        red,
        green,
        blue,
        alpha
    }};

    clear_image(
        command,
        images_[image_index],
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        &color,
        1,
        &range);

    const VkImageMemoryBarrier
        to_present{
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            nullptr,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            0,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            images_[image_index],
            range
        };

    barrier(
        command,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &to_present);

    status =
        end_command(command);

    if (status != VK_SUCCESS) {
        return fail_after_fence_reset(
            result_message(
                "vkEndCommandBuffer",
                status));
    }

    void* wait_semaphore =
        image_available_;

    void* signal_semaphore =
        render_finished_;

    constexpr std::uint32_t
        wait_stage =
            VK_PIPELINE_STAGE_TRANSFER_BIT;

    const VkSubmitInfo submit{
        VK_STRUCTURE_TYPE_SUBMIT_INFO,
        nullptr,
        1,
        &wait_semaphore,
        &wait_stage,
        1,
        &command,
        1,
        &signal_semaphore
    };

    status =
        queue_submit(
            queue_,
            1,
            &submit,
            frame_fence_);

    if (status != VK_SUCCESS) {
        return fail_after_fence_reset(
            result_message(
                "vkQueueSubmit",
                status));
    }

    void* swapchain =
        swapchain_;

    const VkPresentInfoKHR present{
        VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        nullptr,
        1,
        &signal_semaphore,
        1,
        &swapchain,
        &image_index,
        nullptr
    };

    status =
        queue_present(
            queue_,
            &present);

    image_initialized_[
        image_index] = true;

    if (status ==
            VK_ERROR_OUT_OF_DATE_KHR ||
        status ==
            VK_SUBOPTIMAL_KHR) {

        needs_resize_ = true;

        diagnostic_ =
            status ==
                VK_ERROR_OUT_OF_DATE_KHR
                ? "Vulkan swapchain became out of date during present"
                : "Vulkan swapchain became suboptimal during present";

        return false;
    }

    if (status != VK_SUCCESS) {
        diagnostic_ =
            result_message(
                "vkQueuePresentKHR",
                status);
        return false;
    }

    diagnostic_ =
        "Vulkan clear frame presented";

    return true;
}

void VulkanClearPresenter::shutdown() noexcept {
    if (!device_api_ ||
        !device_) {

        device_api_ = nullptr;
        device_ = nullptr;
        queue_ = nullptr;
        swapchain_ = nullptr;
        images_.clear();
        command_buffers_.clear();
        image_initialized_.clear();
        command_pool_ = nullptr;
        image_available_ = nullptr;
        render_finished_ = nullptr;
        frame_fence_ = nullptr;
        needs_resize_ = false;
        return;
    }

    const auto wait_idle =
        load_proc<DeviceWaitIdle>(
            *device_api_,
            "vkDeviceWaitIdle");

    if (wait_idle) {
        wait_idle(device_);
    }

    const auto destroy_fence =
        load_proc<DestroyFence>(
            *device_api_,
            "vkDestroyFence");

    const auto destroy_semaphore =
        load_proc<DestroySemaphore>(
            *device_api_,
            "vkDestroySemaphore");

    const auto destroy_pool =
        load_proc<DestroyCommandPool>(
            *device_api_,
            "vkDestroyCommandPool");

    if (destroy_fence &&
        frame_fence_) {

        destroy_fence(
            device_,
            frame_fence_,
            nullptr);
    }

    if (destroy_semaphore &&
        render_finished_) {

        destroy_semaphore(
            device_,
            render_finished_,
            nullptr);
    }

    if (destroy_semaphore &&
        image_available_) {

        destroy_semaphore(
            device_,
            image_available_,
            nullptr);
    }

    if (destroy_pool &&
        command_pool_) {

        destroy_pool(
            device_,
            command_pool_,
            nullptr);
    }

    device_api_ = nullptr;
    device_ = nullptr;
    queue_ = nullptr;
    swapchain_ = nullptr;
    images_.clear();
    command_buffers_.clear();
    image_initialized_.clear();
    command_pool_ = nullptr;
    image_available_ = nullptr;
    render_finished_ = nullptr;
    frame_fence_ = nullptr;
    needs_resize_ = false;
}

} // namespace nengine::render
