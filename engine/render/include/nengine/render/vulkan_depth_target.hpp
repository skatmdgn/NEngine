#pragma once

#include <cstdint>
#include <string>

#include "nengine/render/vulkan_device.hpp"
#include "nengine/render/vulkan_instance.hpp"
#include "nengine/render/vulkan_loader.hpp"

namespace nengine::render {

class VulkanDepthTarget {
public:
    VulkanDepthTarget() = default;
    ~VulkanDepthTarget();

    VulkanDepthTarget(
        const VulkanDepthTarget&) = delete;

    VulkanDepthTarget& operator=(
        const VulkanDepthTarget&) = delete;

    VulkanDepthTarget(
        VulkanDepthTarget&& other) noexcept;

    VulkanDepthTarget& operator=(
        VulkanDepthTarget&& other) noexcept;

    bool create(
        const VulkanLoader& loader,
        const VulkanInstance& instance,
        const VulkanDevice& device,
        std::uint32_t width,
        std::uint32_t height);

    void destroy() noexcept;

    bool valid() const noexcept {
        return image_ != nullptr &&
            memory_ != nullptr &&
            view_ != nullptr &&
            format_ != 0;
    }

    void* native_image() const noexcept {
        return image_;
    }

    void* native_view() const noexcept {
        return view_;
    }

    std::uint32_t format() const noexcept {
        return format_;
    }

    const std::string& diagnostic() const noexcept {
        return diagnostic_;
    }

private:
    const VulkanDevice* device_api_{nullptr};
    void* device_{nullptr};
    void* image_{nullptr};
    void* memory_{nullptr};
    void* view_{nullptr};
    std::uint32_t format_{0};
    std::string diagnostic_{};
};

} // namespace nengine::render
