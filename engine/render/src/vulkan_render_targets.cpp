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
VK_IMAGE_VIEW_TYPE_2D = 1;

constexpr std::uint32_t
VK_COMPONENT_SWIZZLE_IDENTITY = 0;

constexpr std::uint32_t
VK_IMAGE_ASPECT_COLOR_BIT = 0x00000001u;

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
    const VulkanLoader& loader,
    const VulkanInstance& instance,
    const VulkanDevice& device,
    const VulkanSwapchain& swapchain) {

    destroy();
    diagnostic_.clear();

    if (!loader.loaded() ||
        !instance.valid() ||
        !device.valid() ||
        !swapchain.valid()) {

        diagnostic_ =
            "valid Vulkan runtime device and swapchain are required";
        return false;
    }

    const auto create_view =
        load_proc<CreateImageView>(
            device,
            "vkCreateImageView");

    const auto create_framebuffer =
        load_proc<CreateFramebuffer>(
            device,
            "vkCreateFramebuffer");

    if (!create_view ||
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

    if (!depth_target_.create(
            loader,
            instance,
            device,
            swapchain.width(),
            swapchain.height())) {

        diagnostic_ =
            depth_target_.diagnostic();

        destroy();
        return false;
    }

    if (!render_pass_.create_color_depth(
            device,
            swapchain.format(),
            depth_target_.format())) {

        diagnostic_ =
            render_pass_.diagnostic();

        destroy();
        return false;
    }

    framebuffers_.reserve(
        image_views_.size());

    for (auto* view :
         image_views_) {

        void* attachments[] = {
            view,
            depth_target_.native_view()
        };

        const VkFramebufferCreateInfo
            framebuffer_info{
                VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                nullptr,
                0,
                render_pass_
                    .native_handle(),
                2,
                attachments,
                swapchain.width(),
                swapchain.height(),
                1
            };

        void* framebuffer = nullptr;

        const auto status =
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
        "Vulkan color+depth render targets created";

    return true;
}

void VulkanRenderTargets::destroy() noexcept {
    if (device_api_ &&
        device_) {

        const auto destroy_framebuffer =
            load_proc<DestroyFramebuffer>(
                *device_api_,
                "vkDestroyFramebuffer");

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

        framebuffers_.clear();

        render_pass_.destroy();
        depth_target_.destroy();

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
    } else {
        render_pass_.destroy();
        depth_target_.destroy();
        framebuffers_.clear();
    }

    image_views_.clear();
    device_ = nullptr;
    device_api_ = nullptr;
}

} // namespace nengine::render
