#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "nengine/render/vulkan_loader.hpp"

namespace nengine::render {

struct VulkanExtensionInfo {
    std::string name{};
    std::uint32_t specification_version{0};
};

class VulkanInstance {
public:
    VulkanInstance() = default;
    ~VulkanInstance();

    VulkanInstance(
        const VulkanInstance&) = delete;

    VulkanInstance& operator=(
        const VulkanInstance&) = delete;

    VulkanInstance(
        VulkanInstance&& other) noexcept;

    VulkanInstance& operator=(
        VulkanInstance&& other) noexcept;

    bool create(
        const VulkanLoader& loader,
        std::string_view application_name,
        const std::vector<std::string>&
            required_extensions = {});

    void destroy() noexcept;

    bool valid() const noexcept {
        return instance_ != nullptr;
    }

    void* native_handle() const noexcept {
        return instance_;
    }

    const std::string& diagnostic() const noexcept {
        return diagnostic_;
    }

    static std::vector<VulkanExtensionInfo>
    enumerate_extensions(
        const VulkanLoader& loader,
        std::string* error = nullptr);

private:
    using DestroyInstance =
        void (*)(void*, const void*);

    void* instance_{nullptr};
    DestroyInstance destroy_instance_{nullptr};
    std::string diagnostic_{};
};

} // namespace nengine::render
