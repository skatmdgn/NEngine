#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "nengine/render/vulkan_instance.hpp"
#include "nengine/render/vulkan_loader.hpp"

namespace nengine::render {

struct VulkanDeviceExtensionInfo {
    std::string name{};
    std::uint32_t specification_version{0};
};

class VulkanDevice {
public:
    VulkanDevice() = default;
    ~VulkanDevice();

    VulkanDevice(
        const VulkanDevice&) = delete;

    VulkanDevice& operator=(
        const VulkanDevice&) = delete;

    VulkanDevice(
        VulkanDevice&& other) noexcept;

    VulkanDevice& operator=(
        VulkanDevice&& other) noexcept;

    bool create(
        const VulkanLoader& loader,
        const VulkanInstance& instance,
        const std::vector<std::string>&
            required_extensions = {},
        void* presentation_surface = nullptr);

    void destroy() noexcept;

    bool valid() const noexcept {
        return device_ != nullptr;
    }

    void* native_device() const noexcept {
        return device_;
    }

    void* physical_device() const noexcept {
        return physical_device_;
    }

    void* graphics_queue() const noexcept {
        return graphics_queue_;
    }

    VulkanLoader::Function get_proc_address(
        const char* name) const noexcept;

    std::uint32_t graphics_queue_family() const noexcept {
        return graphics_queue_family_;
    }

    const std::string& diagnostic() const noexcept {
        return diagnostic_;
    }

    static std::vector<VulkanDeviceExtensionInfo>
    enumerate_extensions(
        const VulkanLoader& loader,
        const VulkanInstance& instance,
        void* physical_device,
        std::string* error = nullptr);

private:
    using DestroyDevice =
        void (*)(void*, const void*);

    using GetDeviceProcAddr =
        VulkanLoader::Function (*)(
            void*,
            const char*);

    void* physical_device_{nullptr};
    void* device_{nullptr};
    void* graphics_queue_{nullptr};
    std::uint32_t graphics_queue_family_{
        0xFFFFFFFFu};
    DestroyDevice destroy_device_{nullptr};
    GetDeviceProcAddr get_device_proc_addr_{nullptr};
    std::string diagnostic_{};
};

} // namespace nengine::render
