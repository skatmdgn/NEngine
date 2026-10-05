#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "nengine/render/vulkan_device.hpp"
#include "nengine/render/vulkan_instance.hpp"
#include "nengine/render/vulkan_loader.hpp"

namespace nengine::render {

enum class VulkanBufferUsage : std::uint32_t {
    Vertex = 1u << 0u,
    Index = 1u << 1u,
    Uniform = 1u << 2u,
    Storage = 1u << 3u,
    TransferSource = 1u << 4u,
    TransferDestination = 1u << 5u,
};

constexpr VulkanBufferUsage operator|(
    VulkanBufferUsage a,
    VulkanBufferUsage b) noexcept {

    return static_cast<VulkanBufferUsage>(
        static_cast<std::uint32_t>(a) |
        static_cast<std::uint32_t>(b));
}

enum class VulkanMemoryPreference : std::uint8_t {
    HostVisible,
    DeviceLocal,
};

class VulkanBufferResource {
public:
    VulkanBufferResource() = default;
    ~VulkanBufferResource();

    VulkanBufferResource(
        const VulkanBufferResource&) = delete;

    VulkanBufferResource& operator=(
        const VulkanBufferResource&) = delete;

    VulkanBufferResource(
        VulkanBufferResource&& other) noexcept;

    VulkanBufferResource& operator=(
        VulkanBufferResource&& other) noexcept;

    bool create(
        const VulkanLoader& loader,
        const VulkanInstance& instance,
        const VulkanDevice& device,
        std::size_t size_bytes,
        VulkanBufferUsage usage,
        VulkanMemoryPreference memory,
        const void* initial_data = nullptr);

    bool upload(
        const void* data,
        std::size_t size_bytes,
        std::size_t offset = 0);

    void destroy() noexcept;

    bool valid() const noexcept {
        return buffer_ != nullptr &&
            memory_ != nullptr;
    }

    void* native_buffer() const noexcept {
        return buffer_;
    }

    std::size_t size_bytes() const noexcept {
        return size_bytes_;
    }

    bool host_visible() const noexcept {
        return host_visible_;
    }

    const std::string& diagnostic() const noexcept {
        return diagnostic_;
    }

private:
    const VulkanDevice* device_api_{nullptr};
    void* device_{nullptr};
    void* buffer_{nullptr};
    void* memory_{nullptr};
    std::size_t size_bytes_{0};
    bool host_visible_{false};
    std::string diagnostic_{};
};

} // namespace nengine::render
