#include "nengine/render/vulkan_shader.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>

namespace nengine::render {
namespace {

using VkResult = std::int32_t;

constexpr VkResult VK_SUCCESS = 0;

constexpr std::uint32_t
VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO = 16;

constexpr std::uint32_t
SPIRV_MAGIC = 0x07230203u;

struct VkShaderModuleCreateInfo {
    std::uint32_t sType;
    const void* pNext;
    std::uint32_t flags;
    std::size_t codeSize;
    const std::uint32_t* pCode;
};

using CreateShaderModule =
    VkResult (*)(
        void*,
        const VkShaderModuleCreateInfo*,
        const void*,
        void**);

using DestroyShaderModule =
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

VulkanShaderModule::~VulkanShaderModule() {
    destroy();
}

VulkanShaderModule::VulkanShaderModule(
    VulkanShaderModule&& other) noexcept
    : device_api_(
          std::exchange(
              other.device_api_,
              nullptr)),
      device_(
          std::exchange(
              other.device_,
              nullptr)),
      module_(
          std::exchange(
              other.module_,
              nullptr)),
      stage_(
          other.stage_),
      diagnostic_(
          std::move(
              other.diagnostic_)) {}

VulkanShaderModule&
VulkanShaderModule::operator=(
    VulkanShaderModule&& other) noexcept {

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

    module_ =
        std::exchange(
            other.module_,
            nullptr);

    stage_ =
        other.stage_;

    diagnostic_ =
        std::move(
            other.diagnostic_);

    return *this;
}

bool VulkanShaderModule::create(
    const VulkanDevice& device,
    VulkanShaderStage stage,
    const std::vector<std::uint32_t>& spirv) {

    destroy();
    diagnostic_.clear();

    if (!device.valid()) {
        diagnostic_ =
            "valid Vulkan device is required";
        return false;
    }

    if (spirv.size() < 5 ||
        spirv.front() != SPIRV_MAGIC) {

        diagnostic_ =
            "valid SPIR-V binary header is required";
        return false;
    }

    const auto create_shader =
        load_proc<CreateShaderModule>(
            device,
            "vkCreateShaderModule");

    if (!create_shader) {
        diagnostic_ =
            "vkCreateShaderModule is unavailable";
        return false;
    }

    const VkShaderModuleCreateInfo info{
        VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        nullptr,
        0,
        spirv.size() *
            sizeof(std::uint32_t),
        spirv.data()
    };

    void* module = nullptr;

    const auto status =
        create_shader(
            device.native_device(),
            &info,
            nullptr,
            &module);

    if (status != VK_SUCCESS ||
        !module) {

        diagnostic_ =
            result_message(
                "vkCreateShaderModule",
                status);
        return false;
    }

    device_api_ = &device;
    device_ =
        device.native_device();
    module_ = module;
    stage_ = stage;

    diagnostic_ =
        stage ==
            VulkanShaderStage::Vertex
            ? "Vulkan vertex shader module created"
            : "Vulkan fragment shader module created";

    return true;
}

void VulkanShaderModule::destroy() noexcept {
    if (device_api_ &&
        device_ &&
        module_) {

        const auto destroy_shader =
            load_proc<DestroyShaderModule>(
                *device_api_,
                "vkDestroyShaderModule");

        if (destroy_shader) {
            destroy_shader(
                device_,
                module_,
                nullptr);
        }
    }

    device_api_ = nullptr;
    device_ = nullptr;
    module_ = nullptr;
}

} // namespace nengine::render
