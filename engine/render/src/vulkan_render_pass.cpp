#include "nengine/render/vulkan_render_pass.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace nengine::render {
namespace {

using VkResult = std::int32_t;

constexpr VkResult VK_SUCCESS = 0;

constexpr std::uint32_t
VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO = 38;

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
VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL = 3;

constexpr std::uint32_t
VK_IMAGE_LAYOUT_PRESENT_SRC_KHR = 1000001002u;

constexpr std::uint32_t
VK_PIPELINE_BIND_POINT_GRAPHICS = 0;

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

VulkanRenderPass::~VulkanRenderPass() {
    destroy();
}

VulkanRenderPass::VulkanRenderPass(
    VulkanRenderPass&& other) noexcept
    : device_api_(
          std::exchange(
              other.device_api_,
              nullptr)),
      device_(
          std::exchange(
              other.device_,
              nullptr)),
      render_pass_(
          std::exchange(
              other.render_pass_,
              nullptr)),
      color_format_(
          std::exchange(
              other.color_format_,
              0)),
      depth_format_(
          std::exchange(
              other.depth_format_,
              0)),
      diagnostic_(
          std::move(
              other.diagnostic_)) {}

VulkanRenderPass&
VulkanRenderPass::operator=(
    VulkanRenderPass&& other) noexcept {

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

    render_pass_ =
        std::exchange(
            other.render_pass_,
            nullptr);

    color_format_ =
        std::exchange(
            other.color_format_,
            0);

    depth_format_ =
        std::exchange(
            other.depth_format_,
            0);

    diagnostic_ =
        std::move(
            other.diagnostic_);

    return *this;
}

bool VulkanRenderPass::create_color(
    const VulkanDevice& device,
    std::uint32_t color_format) {

    return create_color_depth(
        device,
        color_format,
        0);
}

bool VulkanRenderPass::create_color_depth(
    const VulkanDevice& device,
    std::uint32_t color_format,
    std::uint32_t depth_format) {

    destroy();
    diagnostic_.clear();

    if (!device.valid() ||
        color_format == 0) {

        diagnostic_ =
            "valid Vulkan device and color format are required";
        return false;
    }

    const auto create_render_pass =
        load_proc<CreateRenderPass>(
            device,
            "vkCreateRenderPass");

    if (!create_render_pass) {
        diagnostic_ =
            "vkCreateRenderPass is unavailable";
        return false;
    }

    std::array<
        VkAttachmentDescription,
        2> attachments{};

    attachments[0] = {
        0,
        color_format,
        VK_SAMPLE_COUNT_1_BIT,
        VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_STORE,
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        VK_ATTACHMENT_STORE_OP_DONT_CARE,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    };

    const VkAttachmentReference
        color_reference{
            0,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        };

    VkAttachmentReference
        depth_reference{
            1,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        };

    std::uint32_t attachment_count = 1;

    if (depth_format != 0) {
        attachments[1] = {
            0,
            depth_format,
            VK_SAMPLE_COUNT_1_BIT,
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        };

        attachment_count = 2;
    }

    const VkSubpassDescription subpass{
        0,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        0,
        nullptr,
        1,
        &color_reference,
        nullptr,
        depth_format != 0
            ? &depth_reference
            : nullptr,
        0,
        nullptr
    };

    const VkRenderPassCreateInfo info{
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        nullptr,
        0,
        attachment_count,
        attachments.data(),
        1,
        &subpass,
        0,
        nullptr
    };

    void* render_pass = nullptr;

    const auto status =
        create_render_pass(
            device.native_device(),
            &info,
            nullptr,
            &render_pass);

    if (status != VK_SUCCESS ||
        !render_pass) {

        diagnostic_ =
            result_message(
                "vkCreateRenderPass",
                status);
        return false;
    }

    device_api_ = &device;
    device_ =
        device.native_device();
    render_pass_ =
        render_pass;
    color_format_ =
        color_format;
    depth_format_ =
        depth_format;

    diagnostic_ =
        depth_format_ != 0
            ? "Vulkan color+depth render pass created"
            : "Vulkan single-color render pass created";

    return true;
}

void VulkanRenderPass::destroy() noexcept {
    if (device_api_ &&
        device_ &&
        render_pass_) {

        const auto destroy_render_pass =
            load_proc<DestroyRenderPass>(
                *device_api_,
                "vkDestroyRenderPass");

        if (destroy_render_pass) {
            destroy_render_pass(
                device_,
                render_pass_,
                nullptr);
        }
    }

    device_api_ = nullptr;
    device_ = nullptr;
    render_pass_ = nullptr;
    color_format_ = 0;
    depth_format_ = 0;
}

} // namespace nengine::render
