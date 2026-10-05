#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "nengine/render/vulkan_device.hpp"
#include "nengine/render/vulkan_instance.hpp"
#include "nengine/render/vulkan_loader.hpp"

namespace nengine::render {

enum class VulkanTextureColorSpace : std::uint8_t {
    Linear,
    SRgb,
};

class VulkanTextureResource {
public:
    VulkanTextureResource() = default;
    ~VulkanTextureResource();

    VulkanTextureResource(
        const VulkanTextureResource&) = delete;

    VulkanTextureResource& operator=(
        const VulkanTextureResource&) = delete;

    VulkanTextureResource(
        VulkanTextureResource&& other) noexcept;

    VulkanTextureResource& operator=(
        VulkanTextureResource&& other) noexcept;

    bool create_rgba8(
        const VulkanLoader& loader,
        const VulkanInstance& instance,
        const VulkanDevice& device,
        std::uint32_t width,
        std::uint32_t height,
        const void* pixels,
        std::size_t pixel_bytes,
        VulkanTextureColorSpace color_space =
            VulkanTextureColorSpace::SRgb);

    void destroy() noexcept;

    bool valid() const noexcept {
        return image_ != nullptr &&
            memory_ != nullptr &&
            view_ != nullptr &&
            sampler_ != nullptr;
    }

    void* native_image() const noexcept {
        return image_;
    }

    void* native_view() const noexcept {
        return view_;
    }

    void* native_sampler() const noexcept {
        return sampler_;
    }

    std::uint32_t width() const noexcept {
        return width_;
    }

    std::uint32_t height() const noexcept {
        return height_;
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
    void* sampler_{nullptr};
    std::uint32_t width_{0};
    std::uint32_t height_{0};
    std::uint32_t format_{0};
    std::string diagnostic_{};
};

} // namespace nengine::render
