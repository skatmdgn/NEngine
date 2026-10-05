#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "nengine/render/vulkan_device.hpp"

namespace nengine::render {

enum class VulkanShaderStage : std::uint8_t {
    Vertex,
    Fragment,
};

class VulkanShaderModule {
public:
    VulkanShaderModule() = default;
    ~VulkanShaderModule();

    VulkanShaderModule(
        const VulkanShaderModule&) = delete;

    VulkanShaderModule& operator=(
        const VulkanShaderModule&) = delete;

    VulkanShaderModule(
        VulkanShaderModule&& other) noexcept;

    VulkanShaderModule& operator=(
        VulkanShaderModule&& other) noexcept;

    bool create(
        const VulkanDevice& device,
        VulkanShaderStage stage,
        const std::vector<std::uint32_t>& spirv);

    void destroy() noexcept;

    bool valid() const noexcept {
        return module_ != nullptr;
    }

    VulkanShaderStage stage() const noexcept {
        return stage_;
    }

    void* native_module() const noexcept {
        return module_;
    }

    const std::string& diagnostic() const noexcept {
        return diagnostic_;
    }

private:
    const VulkanDevice* device_api_{nullptr};
    void* device_{nullptr};
    void* module_{nullptr};
    VulkanShaderStage stage_{
        VulkanShaderStage::Vertex};
    std::string diagnostic_{};
};

} // namespace nengine::render
