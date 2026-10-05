#pragma once

#include <string>

namespace nengine::render {

class VulkanLoader {
public:
    VulkanLoader();
    ~VulkanLoader();

    VulkanLoader(
        const VulkanLoader&) = delete;

    VulkanLoader& operator=(
        const VulkanLoader&) = delete;

    VulkanLoader(
        VulkanLoader&& other) noexcept;

    VulkanLoader& operator=(
        VulkanLoader&& other) noexcept;

    bool loaded() const noexcept {
        return library_ != nullptr &&
            get_instance_proc_addr_ != nullptr;
    }

    void* get_proc_address(
        const char* name) const noexcept;

    const std::string& diagnostic() const noexcept {
        return diagnostic_;
    }

private:
    using GetInstanceProcAddr =
        void* (*)(void*, const char*);

    void close() noexcept;

    void* library_{nullptr};
    GetInstanceProcAddr
        get_instance_proc_addr_{nullptr};
    std::string diagnostic_{};
};

} // namespace nengine::render
