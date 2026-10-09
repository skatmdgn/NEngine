#include "nengine/render/vulkan_pipeline.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "nengine/render/matrix.hpp"
#include "nengine/render/mesh_data.hpp"

namespace nengine::render {
namespace {

using VkResult = std::int32_t;
using VkBool32 = std::uint32_t;

constexpr VkResult VK_SUCCESS = 0;

constexpr std::uint32_t
VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO = 18;

constexpr std::uint32_t
VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO = 19;

constexpr std::uint32_t
VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO = 20;

constexpr std::uint32_t
VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO = 22;

constexpr std::uint32_t
VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO = 23;

constexpr std::uint32_t
VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO = 24;

constexpr std::uint32_t
VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO = 25;

constexpr std::uint32_t
VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO = 26;

constexpr std::uint32_t
VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO = 27;

constexpr std::uint32_t
VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO = 28;

constexpr std::uint32_t
VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO = 30;

constexpr std::uint32_t
VK_SHADER_STAGE_VERTEX_BIT = 0x00000001u;

constexpr std::uint32_t
VK_SHADER_STAGE_FRAGMENT_BIT = 0x00000010u;

constexpr std::uint32_t
VK_VERTEX_INPUT_RATE_VERTEX = 0;

constexpr std::uint32_t
VK_FORMAT_R32G32_SFLOAT = 103;

constexpr std::uint32_t
VK_FORMAT_R32G32B32_SFLOAT = 106;

constexpr std::uint32_t
VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST = 3;

constexpr std::uint32_t
VK_POLYGON_MODE_FILL = 0;

constexpr std::uint32_t
VK_CULL_MODE_NONE = 0u;

constexpr std::uint32_t
VK_CULL_MODE_BACK_BIT = 0x00000002u;

constexpr std::uint32_t
VK_FRONT_FACE_COUNTER_CLOCKWISE = 0;

constexpr std::uint32_t
VK_SAMPLE_COUNT_1_BIT = 0x00000001u;

constexpr std::uint32_t
VK_COMPARE_OP_LESS = 1u;

constexpr std::uint32_t
VK_BLEND_FACTOR_ZERO = 0u;

constexpr std::uint32_t
VK_BLEND_FACTOR_ONE = 1u;

constexpr std::uint32_t
VK_BLEND_FACTOR_SRC_ALPHA = 6u;

constexpr std::uint32_t
VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA = 7u;

constexpr std::uint32_t
VK_BLEND_OP_ADD = 0u;

constexpr std::uint32_t
VK_COLOR_COMPONENT_R_BIT = 0x00000001u;

constexpr std::uint32_t
VK_COLOR_COMPONENT_G_BIT = 0x00000002u;

constexpr std::uint32_t
VK_COLOR_COMPONENT_B_BIT = 0x00000004u;

constexpr std::uint32_t
VK_COLOR_COMPONENT_A_BIT = 0x00000008u;

constexpr std::uint32_t
VK_DYNAMIC_STATE_VIEWPORT = 0;

constexpr std::uint32_t
VK_DYNAMIC_STATE_SCISSOR = 1;

struct VkPipelineShaderStageCreateInfo {
    std::uint32_t sType;
    const void* pNext;
    std::uint32_t flags;
    std::uint32_t stage;
    void* module;
    const char* pName;
    const void* pSpecializationInfo;
};

struct VkVertexInputBindingDescription {
    std::uint32_t binding;
    std::uint32_t stride;
    std::uint32_t inputRate;
};

struct VkVertexInputAttributeDescription {
    std::uint32_t location;
    std::uint32_t binding;
    std::uint32_t format;
    std::uint32_t offset;
};

struct VkPipelineVertexInputStateCreateInfo {
    std::uint32_t sType;
    const void* pNext;
    std::uint32_t flags;
    std::uint32_t vertexBindingDescriptionCount;
    const VkVertexInputBindingDescription*
        pVertexBindingDescriptions;
    std::uint32_t vertexAttributeDescriptionCount;
    const VkVertexInputAttributeDescription*
        pVertexAttributeDescriptions;
};

struct VkPipelineInputAssemblyStateCreateInfo {
    std::uint32_t sType;
    const void* pNext;
    std::uint32_t flags;
    std::uint32_t topology;
    VkBool32 primitiveRestartEnable;
};

struct VkPipelineViewportStateCreateInfo {
    std::uint32_t sType;
    const void* pNext;
    std::uint32_t flags;
    std::uint32_t viewportCount;
    const void* pViewports;
    std::uint32_t scissorCount;
    const void* pScissors;
};

struct VkPipelineRasterizationStateCreateInfo {
    std::uint32_t sType;
    const void* pNext;
    std::uint32_t flags;
    VkBool32 depthClampEnable;
    VkBool32 rasterizerDiscardEnable;
    std::uint32_t polygonMode;
    std::uint32_t cullMode;
    std::uint32_t frontFace;
    VkBool32 depthBiasEnable;
    float depthBiasConstantFactor;
    float depthBiasClamp;
    float depthBiasSlopeFactor;
    float lineWidth;
};

struct VkPipelineMultisampleStateCreateInfo {
    std::uint32_t sType;
    const void* pNext;
    std::uint32_t flags;
    std::uint32_t rasterizationSamples;
    VkBool32 sampleShadingEnable;
    float minSampleShading;
    const std::uint32_t* pSampleMask;
    VkBool32 alphaToCoverageEnable;
    VkBool32 alphaToOneEnable;
};

struct VkStencilOpState {
    std::uint32_t failOp;
    std::uint32_t passOp;
    std::uint32_t depthFailOp;
    std::uint32_t compareOp;
    std::uint32_t compareMask;
    std::uint32_t writeMask;
    std::uint32_t reference;
};

struct VkPipelineDepthStencilStateCreateInfo {
    std::uint32_t sType;
    const void* pNext;
    std::uint32_t flags;
    VkBool32 depthTestEnable;
    VkBool32 depthWriteEnable;
    std::uint32_t depthCompareOp;
    VkBool32 depthBoundsTestEnable;
    VkBool32 stencilTestEnable;
    VkStencilOpState front;
    VkStencilOpState back;
    float minDepthBounds;
    float maxDepthBounds;
};

struct VkPipelineColorBlendAttachmentState {
    VkBool32 blendEnable;
    std::uint32_t srcColorBlendFactor;
    std::uint32_t dstColorBlendFactor;
    std::uint32_t colorBlendOp;
    std::uint32_t srcAlphaBlendFactor;
    std::uint32_t dstAlphaBlendFactor;
    std::uint32_t alphaBlendOp;
    std::uint32_t colorWriteMask;
};

struct VkPipelineColorBlendStateCreateInfo {
    std::uint32_t sType;
    const void* pNext;
    std::uint32_t flags;
    VkBool32 logicOpEnable;
    std::uint32_t logicOp;
    std::uint32_t attachmentCount;
    const VkPipelineColorBlendAttachmentState*
        pAttachments;
    float blendConstants[4];
};

struct VkPipelineDynamicStateCreateInfo {
    std::uint32_t sType;
    const void* pNext;
    std::uint32_t flags;
    std::uint32_t dynamicStateCount;
    const std::uint32_t* pDynamicStates;
};

struct VkPushConstantRange {
    std::uint32_t stageFlags;
    std::uint32_t offset;
    std::uint32_t size;
};

struct VkPipelineLayoutCreateInfo {
    std::uint32_t sType;
    const void* pNext;
    std::uint32_t flags;
    std::uint32_t setLayoutCount;
    const void* pSetLayouts;
    std::uint32_t pushConstantRangeCount;
    const VkPushConstantRange*
        pPushConstantRanges;
};

struct VkGraphicsPipelineCreateInfo {
    std::uint32_t sType;
    const void* pNext;
    std::uint32_t flags;
    std::uint32_t stageCount;
    const VkPipelineShaderStageCreateInfo*
        pStages;
    const VkPipelineVertexInputStateCreateInfo*
        pVertexInputState;
    const VkPipelineInputAssemblyStateCreateInfo*
        pInputAssemblyState;
    const void* pTessellationState;
    const VkPipelineViewportStateCreateInfo*
        pViewportState;
    const VkPipelineRasterizationStateCreateInfo*
        pRasterizationState;
    const VkPipelineMultisampleStateCreateInfo*
        pMultisampleState;
    const void* pDepthStencilState;
    const VkPipelineColorBlendStateCreateInfo*
        pColorBlendState;
    const VkPipelineDynamicStateCreateInfo*
        pDynamicState;
    void* layout;
    void* renderPass;
    std::uint32_t subpass;
    void* basePipelineHandle;
    std::int32_t basePipelineIndex;
};

using CreatePipelineLayout =
    VkResult (*)(
        void*,
        const VkPipelineLayoutCreateInfo*,
        const void*,
        void**);

using DestroyPipelineLayout =
    void (*)(
        void*,
        void*,
        const void*);

using CreateGraphicsPipelines =
    VkResult (*)(
        void*,
        void*,
        std::uint32_t,
        const VkGraphicsPipelineCreateInfo*,
        const void*,
        void**);

using DestroyPipeline =
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

VulkanGraphicsPipeline::~VulkanGraphicsPipeline() {
    destroy();
}

bool VulkanGraphicsPipeline::create(
    const VulkanDevice& device,
    const VulkanRenderTargets& targets,
    const VulkanShaderModule& vertex_shader,
    const VulkanShaderModule& fragment_shader) {

    return create(
        device,
        targets.render_pass_resource(),
        vertex_shader,
        fragment_shader);
}

bool VulkanGraphicsPipeline::create(
    const VulkanDevice& device,
    const VulkanRenderPass& render_pass,
    const VulkanShaderModule& vertex_shader,
    const VulkanShaderModule& fragment_shader) {

    return create_internal(
        device,
        render_pass,
        vertex_shader,
        fragment_shader,
        nullptr,
        VulkanGraphicsPipelineOptions{});
}

bool VulkanGraphicsPipeline::create(
    const VulkanDevice& device,
    const VulkanRenderPass& render_pass,
    const VulkanShaderModule& vertex_shader,
    const VulkanShaderModule& fragment_shader,
    const VulkanMaterialResource& material) {

    if (!material.valid()) {
        destroy();
        diagnostic_ =
            "valid Vulkan material is required";
        return false;
    }

    return create(
        device,
        render_pass,
        vertex_shader,
        fragment_shader,
        material,
        VulkanGraphicsPipelineOptions{});
}

bool VulkanGraphicsPipeline::create(
    const VulkanDevice& device,
    const VulkanRenderPass& render_pass,
    const VulkanShaderModule& vertex_shader,
    const VulkanShaderModule& fragment_shader,
    const VulkanMaterialResource& material,
    const VulkanGraphicsPipelineOptions& options) {

    if (!material.valid()) {
        destroy();
        diagnostic_ =
            "valid Vulkan material is required";
        return false;
    }

    return create_internal(
        device,
        render_pass,
        vertex_shader,
        fragment_shader,
        material
            .native_descriptor_set_layout(),
        options);
}

bool VulkanGraphicsPipeline::create_internal(
    const VulkanDevice& device,
    const VulkanRenderPass& render_pass,
    const VulkanShaderModule& vertex_shader,
    const VulkanShaderModule& fragment_shader,
    void* descriptor_set_layout,
    const VulkanGraphicsPipelineOptions& options) {

    destroy();
    diagnostic_.clear();

    if (!device.valid() ||
        !render_pass.valid() ||
        !vertex_shader.valid() ||
        !fragment_shader.valid() ||
        vertex_shader.stage() !=
            VulkanShaderStage::Vertex ||
        fragment_shader.stage() !=
            VulkanShaderStage::Fragment) {

        diagnostic_ =
            "valid Vulkan device render pass and vertex/fragment shader modules are required";
        return false;
    }

    const auto create_layout =
        load_proc<CreatePipelineLayout>(
            device,
            "vkCreatePipelineLayout");

    const auto destroy_layout =
        load_proc<DestroyPipelineLayout>(
            device,
            "vkDestroyPipelineLayout");

    const auto create_pipelines =
        load_proc<CreateGraphicsPipelines>(
            device,
            "vkCreateGraphicsPipelines");

    if (!create_layout ||
        !destroy_layout ||
        !create_pipelines) {

        diagnostic_ =
            "required Vulkan graphics-pipeline functions are unavailable";
        return false;
    }

    const VkPushConstantRange push_constant{
        VK_SHADER_STAGE_VERTEX_BIT,
        0,
        static_cast<std::uint32_t>(
            sizeof(Mat4))
    };

    void* descriptor_set_layouts[] = {
        descriptor_set_layout
    };

    const VkPipelineLayoutCreateInfo layout_info{
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        nullptr,
        0,
        descriptor_set_layout
            ? 1u
            : 0u,
        descriptor_set_layout
            ? descriptor_set_layouts
            : nullptr,
        1,
        &push_constant
    };

    void* layout = nullptr;

    auto status =
        create_layout(
            device.native_device(),
            &layout_info,
            nullptr,
            &layout);

    if (status != VK_SUCCESS ||
        !layout) {

        diagnostic_ =
            result_message(
                "vkCreatePipelineLayout",
                status);
        return false;
    }

    const VkPipelineShaderStageCreateInfo stages[] = {
        {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            nullptr,
            0,
            VK_SHADER_STAGE_VERTEX_BIT,
            vertex_shader.native_module(),
            "main",
            nullptr
        },
        {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            nullptr,
            0,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            fragment_shader.native_module(),
            "main",
            nullptr
        }
    };

    const VkVertexInputBindingDescription binding{
        0,
        static_cast<std::uint32_t>(
            sizeof(MeshVertex)),
        VK_VERTEX_INPUT_RATE_VERTEX
    };

    const VkVertexInputAttributeDescription attributes[] = {
        {
            0,
            0,
            VK_FORMAT_R32G32B32_SFLOAT,
            static_cast<std::uint32_t>(
                offsetof(
                    MeshVertex,
                    position))
        },
        {
            1,
            0,
            VK_FORMAT_R32G32B32_SFLOAT,
            static_cast<std::uint32_t>(
                offsetof(
                    MeshVertex,
                    normal))
        },
        {
            2,
            0,
            VK_FORMAT_R32G32_SFLOAT,
            static_cast<std::uint32_t>(
                offsetof(
                    MeshVertex,
                    uv))
        }
    };

    const VkPipelineVertexInputStateCreateInfo vertex_input{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        nullptr,
        0,
        1,
        &binding,
        3,
        attributes
    };

    const VkPipelineInputAssemblyStateCreateInfo input_assembly{
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        nullptr,
        0,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        0
    };

    const VkPipelineViewportStateCreateInfo viewport_state{
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        nullptr,
        0,
        1,
        nullptr,
        1,
        nullptr
    };

    const VkPipelineRasterizationStateCreateInfo rasterization{
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        nullptr,
        0,
        0,
        0,
        VK_POLYGON_MODE_FILL,
        options.back_face_culling
            ? VK_CULL_MODE_BACK_BIT
            : VK_CULL_MODE_NONE,
        VK_FRONT_FACE_COUNTER_CLOCKWISE,
        0,
        0.0f,
        0.0f,
        0.0f,
        1.0f
    };

    const VkPipelineMultisampleStateCreateInfo multisample{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        nullptr,
        0,
        VK_SAMPLE_COUNT_1_BIT,
        0,
        0.0f,
        nullptr,
        0,
        0
    };

    const VkPipelineDepthStencilStateCreateInfo
        depth_stencil{
            VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            nullptr,
            0,
            render_pass.has_depth() &&
                    options.depth_test
                ? 1u
                : 0u,
            render_pass.has_depth() &&
                    options.depth_write
                ? 1u
                : 0u,
            VK_COMPARE_OP_LESS,
            0,
            0,
            {},
            {},
            0.0f,
            1.0f
        };

    const VkPipelineColorBlendAttachmentState
        color_attachment{
            options.alpha_blend
                ? 1u
                : 0u,
            options.alpha_blend
                ? VK_BLEND_FACTOR_SRC_ALPHA
                : VK_BLEND_FACTOR_ZERO,
            options.alpha_blend
                ? VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA
                : VK_BLEND_FACTOR_ZERO,
            VK_BLEND_OP_ADD,
            options.alpha_blend
                ? VK_BLEND_FACTOR_ONE
                : VK_BLEND_FACTOR_ZERO,
            options.alpha_blend
                ? VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA
                : VK_BLEND_FACTOR_ZERO,
            VK_BLEND_OP_ADD,
            VK_COLOR_COMPONENT_R_BIT |
                VK_COLOR_COMPONENT_G_BIT |
                VK_COLOR_COMPONENT_B_BIT |
                VK_COLOR_COMPONENT_A_BIT
        };

    const VkPipelineColorBlendStateCreateInfo color_blend{
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        nullptr,
        0,
        0,
        0,
        1,
        &color_attachment,
        {0.0f, 0.0f, 0.0f, 0.0f}
    };

    const std::uint32_t dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    const VkPipelineDynamicStateCreateInfo dynamic_state{
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        nullptr,
        0,
        2,
        dynamic_states
    };

    const VkGraphicsPipelineCreateInfo pipeline_info{
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        nullptr,
        0,
        2,
        stages,
        &vertex_input,
        &input_assembly,
        nullptr,
        &viewport_state,
        &rasterization,
        &multisample,
        render_pass.has_depth()
            ? &depth_stencil
            : nullptr,
        &color_blend,
        &dynamic_state,
        layout,
        render_pass.native_handle(),
        0,
        nullptr,
        -1
    };

    void* pipeline = nullptr;

    status =
        create_pipelines(
            device.native_device(),
            nullptr,
            1,
            &pipeline_info,
            nullptr,
            &pipeline);

    if (status != VK_SUCCESS ||
        !pipeline) {

        destroy_layout(
            device.native_device(),
            layout,
            nullptr);

        diagnostic_ =
            result_message(
                "vkCreateGraphicsPipelines",
                status);

        return false;
    }

    device_api_ = &device;
    device_ =
        device.native_device();
    layout_ = layout;
    pipeline_ = pipeline;
    material_descriptor_layout_ =
        descriptor_set_layout != nullptr;

    diagnostic_ =
        descriptor_set_layout
            ? "Vulkan MeshVertex textured graphics pipeline created"
            : "Vulkan MeshVertex graphics pipeline created";

    return true;
}

void VulkanGraphicsPipeline::destroy() noexcept {
    if (device_api_ &&
        device_) {

        const auto destroy_pipeline =
            load_proc<DestroyPipeline>(
                *device_api_,
                "vkDestroyPipeline");

        const auto destroy_layout =
            load_proc<DestroyPipelineLayout>(
                *device_api_,
                "vkDestroyPipelineLayout");

        if (destroy_pipeline &&
            pipeline_) {

            destroy_pipeline(
                device_,
                pipeline_,
                nullptr);
        }

        if (destroy_layout &&
            layout_) {

            destroy_layout(
                device_,
                layout_,
                nullptr);
        }
    }

    device_api_ = nullptr;
    device_ = nullptr;
    layout_ = nullptr;
    pipeline_ = nullptr;
    material_descriptor_layout_ = false;
}

} // namespace nengine::render
