#include "nengine/render/vulkan_render_targets.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace nengine::render {
namespace {

using VkResult = std::int32_t;

constexpr VkResult VK_SUCCESS = 0;

constexpr std::uint32_t
VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO = 15;

constexpr std::uint32_t
VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO = 37;

constexpr std::uint32_t
VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO = 38;

constexpr std::uint32_t
VK_IMAGE_VIEW_TYPE_2D = 1;

constexpr std::uint32_t
VK_COMPONENT_SWIZZLE_IDENTITY = 0;

constexpr std::uint32_t
VK_IMAGE_ASPECT_COLOR_BIT = 0x00000001u;

constexpr std::uint32_t
VK_SAMPLE_COUNT_1_BIT = 0x00000001u;

constexpr std::uint32_t
VK_ATTACHMENT_LOAD_OP_CLEAR = 1;

constexpr std::uint32_t
VK_ATTACHMENT_STORE_OP_STORE = 0;

constexpr std::uint32_t
VK_ATTACHMENT_LOAD_OP_DONT_CARE = 2;

constexpr std::uint32_t
VK_ATTACHMENT_STORE_OP_DONT_CARE = 1;

constexpr std::uint32_t
VK_IMAGE_LAYOUT_UNDEFINED = 0;

constexpr std::uint32_t
VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL = 2;

constexpr std::uint32_t
VK_IMAGE_LAYOUT_PRESENT_SRC_KHR = 1000001002u;

constexpr std::uint32_t
VK_PIPELINE_BIND_POINT_GRAPHICS = 0;

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

struct VkAttachmentDescription {
    std::uint32_t flags;
    std::uint32_t format;
    std::uint32_t samples;
    std::uint32_t loadOp;
    std::uint32_t storeOp;
    std::uint32_t stencilLoadOp;
    std::uint32_t stencilStoreOp;
    std::uint32_t initialLayout;
    std::uint32_t finalLayout;
};

struct VkAttachmentReference {
    std::uint32_t attachment;
    std::uint32_t layout;
};

struct VkSubpassDescription {
    std::uint32_t flags;
    std::uint32_t pipelineBindPoint;
    std::uint32_t inputAttachmentCount;
    const VkAttachmentReference*
        pInputAttachments;
    std::uint32_t colorAttachmentCount;
    const VkAttachmentReference*
        pColorAttachments;
    const VkAttachmentReference*
        pResolveAttachments;
    const VkAttachmentReference*
        pDepthStencilAttachment;
    std::uint32_t preserveAttachmentCount;
    const std::uint32_t*
        pPreserveAttachments;
};

struct VkRenderPassCreateInfo {
    std::uint32_t sType;
    const void* pNext;
    std::uint32_t flags;
    std::uint32_t attachmentCount;
    const VkAttachmentDescription*
        pAttachments;
    std::uint32_t subpassCount;
    const VkSubpassDescription*
        pSubpasses;
    std::uint32_t dependencyCount;
    const void* pDependencies;
};

struct VkFramebufferCreateInfo {
    std::uint32_t sType;
    const void* pNext;
    std::uint32_t flags;
    void* renderPass;
    std::uint32_t attachmentCount;
    void* const* pAttachments;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t layers;
};

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

using CreateRenderPass =
    VkResult (*)(
        void*,
        const VkRenderPassCreateInfo*,
        const void*,
        void**);

using DestroyRenderPass =
    void (*)(
        void*,
        void*,
        const void*);

using CreateFramebuffer =
    VkResult (*)(
        void*,
        const VkFramebufferCreateInfo*,
        const void*,
        void**);

using DestroyFramebuffer =
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
T load_proc(
    const VulkanDevice& device,
    const char* name) {

    return reinterpret_cast<T>(
        device.get_proc_address(name));
}

} // namespace

VulkanRenderTargets::~VulkanRenderTargets() {
    destroy();
}

bool VulkanRenderTargets::create(
    const VulkanDevice& device,
    const VulkanSwapchain& swapchain) {

    destroy();
    diagnostic_.clear();

    if (!device.valid() ||
        !swapchain.valid()) {

        diagnostic_ =
            "valid Vulkan device and swapchain are required";
        return false;
    }

    const auto create_view =
        load_proc<CreateImageView>(
            device,
            "vkCreateImageView");

    const auto create_render_pass =
        load_proc<CreateRenderPass>(
            device,
            "vkCreateRenderPass");

    const auto create_framebuffer =
        load_proc<CreateFramebuffer>(
            device,
            "vkCreateFramebuffer");

    if (!create_view ||
        !create_render_pass ||
        !create_framebuffer) {

        diagnostic_ =
            "required Vulkan render-target creation functions are unavailable";
        return false;
    }

    device_api_ = &device;
    device_ =
        device.native_device();

    image_views_.reserve(
        swapchain.images().size());

    for (auto* image :
         swapchain.images()) {

        const VkImageViewCreateInfo view_info{
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            nullptr,
            0,
            image,
            VK_IMAGE_VIEW_TYPE_2D,
            swapchain.format(),
            {
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY,
                VK_COMPONENT_SWIZZLE_IDENTITY
            },
            {
                VK_IMAGE_ASPECT_COLOR_BIT,
                0,
                1,
                0,
                1
            }
        };

        void* view = nullptr;

        const auto status =
            create_view(
                device_,
                &view_info,
                nullptr,
                &view);

        if (status != VK_SUCCESS ||
            !view) {

            diagnostic_ =
                result_message(
                    "vkCreateImageView",
                    status);

            destroy();
            return false;
        }

        image_views_.push_back(view);
    }

    const VkAttachmentDescription color_attachment{
        0,
        swapchain.format(),
        VK_SAMPLE_COUNT_1_BIT,
        VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_STORE,
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        VK_ATTACHMENT_STORE_OP_DONT_CARE,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    };

    const VkAttachmentReference color_reference{
        0,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    const VkSubpassDescription subpass{
        0,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        0,
        nullptr,
        1,
        &color_reference,
        nullptr,
        nullptr,
        0,
        nullptr
    };

    const VkRenderPassCreateInfo
        render_pass_info{
            VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            nullptr,
            0,
            1,
            &color_attachment,
            1,
            &subpass,
            0,
            nullptr
        };

    void* render_pass = nullptr;

    auto status =
        create_render_pass(
            device_,
            &render_pass_info,
            nullptr,
            &render_pass);

    if (status != VK_SUCCESS ||
        !render_pass) {

        diagnostic_ =
            result_message(
                "vkCreateRenderPass",
                status);

        destroy();
        return false;
    }

    render_pass_ = render_pass;

    framebuffers_.reserve(
        image_views_.size());

    for (auto* view :
         image_views_) {

        void* attachments[] = {
            view
        };

        const VkFramebufferCreateInfo
            framebuffer_info{
                VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                nullptr,
                0,
                render_pass_,
                1,
                attachments,
                swapchain.width(),
                swapchain.height(),
                1
            };

        void* framebuffer = nullptr;

        status =
            create_framebuffer(
                device_,
                &framebuffer_info,
                nullptr,
                &framebuffer);

        if (status != VK_SUCCESS ||
            !framebuffer) {

            diagnostic_ =
                result_message(
                    "vkCreateFramebuffer",
                    status);

            destroy();
            return false;
        }

        framebuffers_.push_back(
            framebuffer);
    }

    diagnostic_ =
        "Vulkan render targets created";

    return true;
}

void VulkanRenderTargets::destroy() noexcept {
    if (device_api_ &&
        device_) {

        const auto destroy_framebuffer =
            load_proc<DestroyFramebuffer>(
                *device_api_,
                "vkDestroyFramebuffer");

        const auto destroy_render_pass =
            load_proc<DestroyRenderPass>(
                *device_api_,
                "vkDestroyRenderPass");

        const auto destroy_view =
            load_proc<DestroyImageView>(
                *device_api_,
                "vkDestroyImageView");

        if (destroy_framebuffer) {
            for (auto* framebuffer :
                 framebuffers_) {

                if (framebuffer) {
                    destroy_framebuffer(
                        device_,
                        framebuffer,
                        nullptr);
                }
            }
        }

        if (destroy_render_pass &&
            render_pass_) {

            destroy_render_pass(
                device_,
                render_pass_,
                nullptr);
        }

        if (destroy_view) {
            for (auto* view :
                 image_views_) {

                if (view) {
                    destroy_view(
                        device_,
                        view,
                        nullptr);
                }
            }
        }
    }

    framebuffers_.clear();
    image_views_.clear();
    render_pass_ = nullptr;
    device_ = nullptr;
    device_api_ = nullptr;
}

} // namespace nengine::render
