#include "nengine/render/vulkan_material.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nengine::render {
namespace {

using VkResult = std::int32_t;

constexpr VkResult VK_SUCCESS = 0;

constexpr std::uint32_t
VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO = 32;
constexpr std::uint32_t
VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO = 33;
constexpr std::uint32_t
VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO = 34;
constexpr std::uint32_t
VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET = 35;

constexpr std::uint32_t
VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER = 1;

constexpr std::uint32_t
VK_SHADER_STAGE_FRAGMENT_BIT = 0x00000010u;

constexpr std::uint32_t
VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL = 5;

struct VkDescriptorSetLayoutBinding {
    std::uint32_t binding;
    std::uint32_t descriptorType;
    std::uint32_t descriptorCount;
    std::uint32_t stageFlags;
    const void* pImmutableSamplers;
};

struct VkDescriptorSetLayoutCreateInfo {
    std::uint32_t sType;
    const void* pNext;
    std::uint32_t flags;
    std::uint32_t bindingCount;
    const VkDescriptorSetLayoutBinding*
        pBindings;
};

struct VkDescriptorPoolSize {
    std::uint32_t type;
    std::uint32_t descriptorCount;
};

struct VkDescriptorPoolCreateInfo {
    std::uint32_t sType;
    const void* pNext;
    std::uint32_t flags;
    std::uint32_t maxSets;
    std::uint32_t poolSizeCount;
    const VkDescriptorPoolSize*
        pPoolSizes;
};

struct VkDescriptorSetAllocateInfo {
    std::uint32_t sType;
    const void* pNext;
    void* descriptorPool;
    std::uint32_t descriptorSetCount;
    void* const* pSetLayouts;
};

struct VkDescriptorImageInfo {
    void* sampler;
    void* imageView;
    std::uint32_t imageLayout;
};

struct VkWriteDescriptorSet {
    std::uint32_t sType;
    const void* pNext;
    void* dstSet;
    std::uint32_t dstBinding;
    std::uint32_t dstArrayElement;
    std::uint32_t descriptorCount;
    std::uint32_t descriptorType;
    const VkDescriptorImageInfo*
        pImageInfo;
    const void* pBufferInfo;
    const void* pTexelBufferView;
};

using CreateDescriptorSetLayout =
    VkResult (*)(
        void*,
        const VkDescriptorSetLayoutCreateInfo*,
        const void*,
        void**);

using DestroyDescriptorSetLayout =
    void (*)(
        void*,
        void*,
        const void*);

using CreateDescriptorPool =
    VkResult (*)(
        void*,
        const VkDescriptorPoolCreateInfo*,
        const void*,
        void**);

using DestroyDescriptorPool =
    void (*)(
        void*,
        void*,
        const void*);

using AllocateDescriptorSets =
    VkResult (*)(
        void*,
        const VkDescriptorSetAllocateInfo*,
        void**);

using UpdateDescriptorSets =
    void (*)(
        void*,
        std::uint32_t,
        const VkWriteDescriptorSet*,
        std::uint32_t,
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

VulkanMaterialResource::~VulkanMaterialResource() {
    destroy();
}

VulkanMaterialResource::VulkanMaterialResource(
    VulkanMaterialResource&& other) noexcept
    : device_api_(
          std::exchange(
              other.device_api_,
              nullptr)),
      device_(
          std::exchange(
              other.device_,
              nullptr)),
      descriptor_set_layout_(
          std::exchange(
              other.descriptor_set_layout_,
              nullptr)),
      descriptor_pool_(
          std::exchange(
              other.descriptor_pool_,
              nullptr)),
      descriptor_set_(
          std::exchange(
              other.descriptor_set_,
              nullptr)),
      diagnostic_(
          std::move(
              other.diagnostic_)) {}

VulkanMaterialResource&
VulkanMaterialResource::operator=(
    VulkanMaterialResource&& other) noexcept {

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

    descriptor_set_layout_ =
        std::exchange(
            other.descriptor_set_layout_,
            nullptr);

    descriptor_pool_ =
        std::exchange(
            other.descriptor_pool_,
            nullptr);

    descriptor_set_ =
        std::exchange(
            other.descriptor_set_,
            nullptr);

    diagnostic_ =
        std::move(
            other.diagnostic_);

    return *this;
}

bool VulkanMaterialResource::create_textured(
    const VulkanDevice& device,
    const VulkanTextureResource& texture) {

    // Keep every sampled material descriptor-set layout compatible
    // with the renderer's fixed five PBR texture bindings. Legacy/Sprite
    // materials only use binding 0 today; duplicating the source texture in
    // the unused slots preserves pipeline-layout compatibility.
    const std::array<
        const VulkanTextureResource*,
        5u>
        textures{
            &texture,
            &texture,
            &texture,
            &texture,
            &texture
        };

    return create_textured_set(
        device,
        textures);
}

bool VulkanMaterialResource::create_textured_set(
    const VulkanDevice& device,
    std::span<
        const VulkanTextureResource* const>
        textures) {

    destroy();
    diagnostic_.clear();

    if (!device.valid() ||
        textures.empty() ||
        textures.size() > 16u) {

        diagnostic_ =
            "valid Vulkan device and 1-16 textures are required";
        return false;
    }

    for (const auto* texture :
         textures) {

        if (!texture ||
            !texture->valid()) {

            diagnostic_ =
                "all Vulkan material textures must be valid";
            return false;
        }
    }

    const auto create_layout =
        load_proc<CreateDescriptorSetLayout>(
            device,
            "vkCreateDescriptorSetLayout");

    const auto destroy_layout =
        load_proc<DestroyDescriptorSetLayout>(
            device,
            "vkDestroyDescriptorSetLayout");

    const auto create_pool =
        load_proc<CreateDescriptorPool>(
            device,
            "vkCreateDescriptorPool");

    const auto destroy_pool =
        load_proc<DestroyDescriptorPool>(
            device,
            "vkDestroyDescriptorPool");

    const auto allocate_sets =
        load_proc<AllocateDescriptorSets>(
            device,
            "vkAllocateDescriptorSets");

    const auto update_sets =
        load_proc<UpdateDescriptorSets>(
            device,
            "vkUpdateDescriptorSets");

    if (!create_layout ||
        !destroy_layout ||
        !create_pool ||
        !destroy_pool ||
        !allocate_sets ||
        !update_sets) {

        diagnostic_ =
            "required Vulkan descriptor functions are unavailable";
        return false;
    }

    std::vector<
        VkDescriptorSetLayoutBinding>
        bindings;

    bindings.reserve(
        textures.size());

    for (std::size_t index = 0u;
         index < textures.size();
         ++index) {

        bindings.push_back({
            static_cast<std::uint32_t>(
                index),
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            1u,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            nullptr
        });
    }

    const VkDescriptorSetLayoutCreateInfo
        layout_info{
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            nullptr,
            0,
            static_cast<std::uint32_t>(
                bindings.size()),
            bindings.data()
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
                "vkCreateDescriptorSetLayout",
                status);
        return false;
    }

    const VkDescriptorPoolSize pool_size{
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        static_cast<std::uint32_t>(
            textures.size())
    };

    const VkDescriptorPoolCreateInfo pool_info{
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        nullptr,
        0,
        1,
        1,
        &pool_size
    };

    void* pool = nullptr;

    status =
        create_pool(
            device.native_device(),
            &pool_info,
            nullptr,
            &pool);

    if (status != VK_SUCCESS ||
        !pool) {

        destroy_layout(
            device.native_device(),
            layout,
            nullptr);

        diagnostic_ =
            result_message(
                "vkCreateDescriptorPool",
                status);
        return false;
    }

    void* layouts[] = {
        layout
    };

    const VkDescriptorSetAllocateInfo
        allocate_info{
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            nullptr,
            pool,
            1,
            layouts
        };

    void* descriptor_set = nullptr;

    status =
        allocate_sets(
            device.native_device(),
            &allocate_info,
            &descriptor_set);

    if (status != VK_SUCCESS ||
        !descriptor_set) {

        destroy_pool(
            device.native_device(),
            pool,
            nullptr);

        destroy_layout(
            device.native_device(),
            layout,
            nullptr);

        diagnostic_ =
            result_message(
                "vkAllocateDescriptorSets",
                status);
        return false;
    }

    std::vector<VkDescriptorImageInfo>
        image_infos;

    image_infos.reserve(
        textures.size());

    for (const auto* texture :
         textures) {

        image_infos.push_back({
            texture->native_sampler(),
            texture->native_view(),
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        });
    }

    std::vector<VkWriteDescriptorSet>
        writes;

    writes.reserve(
        textures.size());

    for (std::size_t index = 0u;
         index < textures.size();
         ++index) {

        writes.push_back({
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            nullptr,
            descriptor_set,
            static_cast<std::uint32_t>(
                index),
            0,
            1,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            &image_infos[index],
            nullptr,
            nullptr
        });
    }

    update_sets(
        device.native_device(),
        static_cast<std::uint32_t>(
            writes.size()),
        writes.data(),
        0,
        nullptr);

    device_api_ = &device;
    device_ =
        device.native_device();
    descriptor_set_layout_ =
        layout;
    descriptor_pool_ =
        pool;
    descriptor_set_ =
        descriptor_set;

    diagnostic_ =
        "Vulkan material descriptor set created with " +
        std::to_string(
            textures.size()) +
        " sampled texture binding(s)";

    return true;
}

void VulkanMaterialResource::destroy() noexcept {
    if (device_api_ &&
        device_) {

        const auto destroy_pool =
            load_proc<DestroyDescriptorPool>(
                *device_api_,
                "vkDestroyDescriptorPool");

        const auto destroy_layout =
            load_proc<DestroyDescriptorSetLayout>(
                *device_api_,
                "vkDestroyDescriptorSetLayout");

        if (destroy_pool &&
            descriptor_pool_) {

            destroy_pool(
                device_,
                descriptor_pool_,
                nullptr);
        }

        if (destroy_layout &&
            descriptor_set_layout_) {

            destroy_layout(
                device_,
                descriptor_set_layout_,
                nullptr);
        }
    }

    device_api_ = nullptr;
    device_ = nullptr;
    descriptor_set_layout_ = nullptr;
    descriptor_pool_ = nullptr;
    descriptor_set_ = nullptr;
}

} // namespace nengine::render
