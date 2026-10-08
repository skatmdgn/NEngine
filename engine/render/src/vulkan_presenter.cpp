#include "nengine/render/vulkan_presenter.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
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
VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO = 43;

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
VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT =
    0x00000400u;

constexpr std::uint32_t
VK_SUBPASS_CONTENTS_INLINE = 0u;

constexpr std::uint32_t
VK_PIPELINE_BIND_POINT_GRAPHICS = 0u;

constexpr std::uint32_t
VK_SHADER_STAGE_VERTEX_BIT = 0x00000001u;

constexpr std::uint32_t
VK_INDEX_TYPE_UINT32 = 1u;

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

struct VkViewport {
    float x;
    float y;
    float width;
    float height;
    float minDepth;
    float maxDepth;
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

struct VkOffset2D {
    std::int32_t x;
    std::int32_t y;
};

struct VkExtent2D {
    std::uint32_t width;
    std::uint32_t height;
};

struct VkRect2D {
    VkOffset2D offset;
    VkExtent2D extent;
};

union VkClearColorValue {
    float float32[4];
    std::int32_t int32[4];
    std::uint32_t uint32[4];
};

union VkClearValue {
    VkClearColorValue color;
    struct {
        float depth;
        std::uint32_t stencil;
    } depthStencil;
};

struct VkRenderPassBeginInfo {
    std::uint32_t sType;
    const void* pNext;
    void* renderPass;
    void* framebuffer;
    VkRect2D renderArea;
    std::uint32_t clearValueCount;
    const VkClearValue* pClearValues;
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
    VkResult (*)(void*);

using CmdBeginRenderPass =
    void (*)(
        void*,
        const VkRenderPassBeginInfo*,
        std::uint32_t);

using CmdEndRenderPass =
    void (*)(void*);

using CmdBindPipeline =
    void (*)(
        void*,
        std::uint32_t,
        void*);

using CmdBindDescriptorSets =
    void (*)(
        void*,
        std::uint32_t,
        void*,
        std::uint32_t,
        std::uint32_t,
        void* const*,
        std::uint32_t,
        const std::uint32_t*);

using CmdSetViewport =
    void (*)(
        void*,
        std::uint32_t,
        std::uint32_t,
        const VkViewport*);

using CmdSetScissor =
    void (*)(
        void*,
        std::uint32_t,
        std::uint32_t,
        const VkRect2D*);

using CmdBindVertexBuffers =
    void (*)(
        void*,
        std::uint32_t,
        std::uint32_t,
        void* const*,
        const std::uint64_t*);

using CmdBindIndexBuffer =
    void (*)(
        void*,
        void*,
        std::uint64_t,
        std::uint32_t);

using CmdPushConstants =
    void (*)(
        void*,
        void*,
        std::uint32_t,
        std::uint32_t,
        std::uint32_t,
        const void*);

using CmdDrawIndexed =
    void (*)(
        void*,
        std::uint32_t,
        std::uint32_t,
        std::uint32_t,
        std::int32_t,
        std::uint32_t);

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
    const VulkanSwapchain& swapchain,
    const VulkanRenderTargets& targets) {

    shutdown();
    diagnostic_.clear();
    needs_resize_ = false;

    if (!device.valid() ||
        !swapchain.valid() ||
        !targets.valid() ||
        targets.count() !=
            swapchain.images().size()) {

        diagnostic_ =
            "valid matching Vulkan device swapchain and render targets are required";
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
        targets.count());

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
    targets_ = &targets;
    device_ =
        device.native_device();
    queue_ =
        device.graphics_queue();
    swapchain_ =
        swapchain.native_handle();
    command_buffers_ =
        std::move(command_buffers);
    command_pool_ = pool;
    image_available_ =
        image_available;
    render_finished_ =
        render_finished;
    frame_fence_ =
        frame_fence;
    width_ =
        swapchain.width();
    height_ =
        swapchain.height();

    diagnostic_ =
        "Vulkan render-pass presenter ready";

    return true;
}

bool VulkanClearPresenter::present_clear(
    float red,
    float green,
    float blue,
    float alpha) {

    return present_frame(
        {},
        red,
        green,
        blue,
        alpha);
}

bool VulkanClearPresenter::present_mesh(
    const VulkanGraphicsPipeline& pipeline,
    const VulkanMeshResource& mesh,
    const Mat4& mvp,
    float clear_red,
    float clear_green,
    float clear_blue,
    float clear_alpha) {

    const VulkanMeshDraw draw{
        &pipeline,
        &mesh,
        mvp
    };

    return present_frame(
        std::span<
            const VulkanMeshDraw>{
                &draw,
                1},
        clear_red,
        clear_green,
        clear_blue,
        clear_alpha);
}

bool VulkanClearPresenter::present_meshes(
    std::span<const VulkanMeshDraw> draws,
    float clear_red,
    float clear_green,
    float clear_blue,
    float clear_alpha) {

    return present_frame(
        draws,
        clear_red,
        clear_green,
        clear_blue,
        clear_alpha);
}

bool VulkanClearPresenter::present_frame(
    std::span<const VulkanMeshDraw> draws,
    float red,
    float green,
    float blue,
    float alpha) {

    if (!ready()) {
        diagnostic_ =
            "Vulkan presenter is not initialized";
        return false;
    }

    const bool draw_mesh =
        !draws.empty();

    bool bind_materials = false;

    for (const auto& draw :
         draws) {

        if (!draw.pipeline ||
            !draw.mesh ||
            !draw.pipeline->valid() ||
            !draw.mesh->valid()) {

            diagnostic_ =
                "all Vulkan mesh draws require valid pipeline mesh and MVP";
            return false;
        }

        const auto available_indices =
            draw.mesh->index_count();

        const auto resolved_count =
            draw.index_count != 0u
                ? draw.index_count
                : available_indices -
                    std::min(
                        draw.first_index,
                        available_indices);

        if (draw.first_index >=
                available_indices ||
            resolved_count == 0u ||
            resolved_count >
                available_indices -
                    draw.first_index) {

            diagnostic_ =
                "Vulkan mesh draw index range is outside the uploaded mesh";
            return false;
        }

        if (draw.material) {
            if (!draw.material->valid()) {
                diagnostic_ =
                    "Vulkan mesh draw material must be valid when provided";
                return false;
            }

            if (!draw.pipeline
                    ->supports_material_descriptors()) {
                diagnostic_ =
                    "Vulkan mesh draw material requires a pipeline created with a material descriptor layout";
                return false;
            }

            bind_materials = true;
        }
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

    const auto begin_render_pass =
        load_proc<CmdBeginRenderPass>(
            *device_api_,
            "vkCmdBeginRenderPass");

    const auto end_render_pass =
        load_proc<CmdEndRenderPass>(
            *device_api_,
            "vkCmdEndRenderPass");

    const auto bind_pipeline =
        draw_mesh
            ? load_proc<CmdBindPipeline>(
                *device_api_,
                "vkCmdBindPipeline")
            : nullptr;

    const auto bind_descriptor_sets =
        bind_materials
            ? load_proc<CmdBindDescriptorSets>(
                *device_api_,
                "vkCmdBindDescriptorSets")
            : nullptr;

    const auto set_viewport =
        draw_mesh
            ? load_proc<CmdSetViewport>(
                *device_api_,
                "vkCmdSetViewport")
            : nullptr;

    const auto set_scissor =
        draw_mesh
            ? load_proc<CmdSetScissor>(
                *device_api_,
                "vkCmdSetScissor")
            : nullptr;

    const auto bind_vertex_buffers =
        draw_mesh
            ? load_proc<CmdBindVertexBuffers>(
                *device_api_,
                "vkCmdBindVertexBuffers")
            : nullptr;

    const auto bind_index_buffer =
        draw_mesh
            ? load_proc<CmdBindIndexBuffer>(
                *device_api_,
                "vkCmdBindIndexBuffer")
            : nullptr;

    const auto push_constants =
        draw_mesh
            ? load_proc<CmdPushConstants>(
                *device_api_,
                "vkCmdPushConstants")
            : nullptr;

    const auto draw_indexed =
        draw_mesh
            ? load_proc<CmdDrawIndexed>(
                *device_api_,
                "vkCmdDrawIndexed")
            : nullptr;

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
        !begin_render_pass ||
        !end_render_pass ||
        !queue_submit ||
        !queue_present ||
        (draw_mesh &&
         (!bind_pipeline ||
          (bind_materials &&
           !bind_descriptor_sets) ||
          !set_viewport ||
          !set_scissor ||
          !bind_vertex_buffers ||
          !bind_index_buffer ||
          !push_constants ||
          !draw_indexed))) {

        diagnostic_ =
            "required Vulkan render-pass frame functions are unavailable";
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
            command_buffers_.size() ||
        image_index >=
            targets_->count()) {

        diagnostic_ =
            "Vulkan acquired image index is outside render-target range";
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

    const VkCommandBufferBeginInfo begin_info{
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

    VkClearValue clear_values[2]{};

    clear_values[0]
        .color
        .float32[0] = red;

    clear_values[0]
        .color
        .float32[1] = green;

    clear_values[0]
        .color
        .float32[2] = blue;

    clear_values[0]
        .color
        .float32[3] = alpha;

    clear_values[1]
        .depthStencil
        .depth = 1.0f;

    clear_values[1]
        .depthStencil
        .stencil = 0;

    const auto clear_count =
        targets_
            ->render_pass_resource()
            .has_depth()
            ? 2u
            : 1u;

    const VkRenderPassBeginInfo render_pass_info{
        VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        nullptr,
        targets_->render_pass(),
        targets_->framebuffer(
            image_index),
        {
            {0, 0},
            {width_, height_}
        },
        clear_count,
        clear_values
    };

    begin_render_pass(
        command,
        &render_pass_info,
        VK_SUBPASS_CONTENTS_INLINE);

    if (draw_mesh) {
        const VkViewport viewport{
            0.0f,
            0.0f,
            static_cast<float>(
                width_),
            static_cast<float>(
                height_),
            0.0f,
            1.0f
        };

        const VkRect2D scissor{
            {0, 0},
            {width_, height_}
        };

        set_viewport(
            command,
            0,
            1,
            &viewport);

        set_scissor(
            command,
            0,
            1,
            &scissor);

        constexpr std::uint64_t
            vertex_offset = 0;

        for (const auto& draw :
             draws) {

            bind_pipeline(
                command,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                draw.pipeline
                    ->native_pipeline());

            if (draw.material) {
                void* descriptor_set =
                    draw.material
                        ->native_descriptor_set();

                bind_descriptor_sets(
                    command,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    draw.pipeline
                        ->native_layout(),
                    0,
                    1,
                    &descriptor_set,
                    0,
                    nullptr);
            }

            void* vertex_buffer =
                draw.mesh
                    ->vertex_buffer()
                    .native_buffer();

            bind_vertex_buffers(
                command,
                0,
                1,
                &vertex_buffer,
                &vertex_offset);

            bind_index_buffer(
                command,
                draw.mesh
                    ->index_buffer()
                    .native_buffer(),
                0,
                VK_INDEX_TYPE_UINT32);

            push_constants(
                command,
                draw.pipeline
                    ->native_layout(),
                VK_SHADER_STAGE_VERTEX_BIT,
                0,
                static_cast<std::uint32_t>(
                    sizeof(Mat4)),
                draw.mvp
                    .value
                    .data());

            const auto index_count =
                draw.index_count != 0u
                    ? draw.index_count
                    : draw.mesh->index_count() -
                        draw.first_index;

            draw_indexed(
                command,
                index_count,
                1,
                draw.first_index,
                0,
                0);
        }
    }

    end_render_pass(command);

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
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

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

    if (draw_mesh) {
        diagnostic_ =
            "Vulkan indexed mesh frame presented (" +
            std::to_string(
                draws.size()) +
            " draw(s))";
    } else {
        diagnostic_ =
            "Vulkan render-pass clear frame presented";
    }

    return true;
}

void VulkanClearPresenter::shutdown() noexcept {
    if (!device_api_ ||
        !device_) {

        device_api_ = nullptr;
        targets_ = nullptr;
        device_ = nullptr;
        queue_ = nullptr;
        swapchain_ = nullptr;
        command_buffers_.clear();
        command_pool_ = nullptr;
        image_available_ = nullptr;
        render_finished_ = nullptr;
        frame_fence_ = nullptr;
        width_ = 0;
        height_ = 0;
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
    targets_ = nullptr;
    device_ = nullptr;
    queue_ = nullptr;
    swapchain_ = nullptr;
    command_buffers_.clear();
    command_pool_ = nullptr;
    image_available_ = nullptr;
    render_finished_ = nullptr;
    frame_fence_ = nullptr;
    width_ = 0;
    height_ = 0;
    needs_resize_ = false;
}

} // namespace nengine::render
