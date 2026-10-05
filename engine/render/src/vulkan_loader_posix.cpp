#include "nengine/render/vulkan_loader.hpp"

#include <dlfcn.h>

#include <utility>

namespace nengine::render {

VulkanLoader::VulkanLoader() {
    library_ =
        dlopen(
            "libvulkan.so.1",
            RTLD_NOW |
                RTLD_LOCAL);

    if (!library_) {
        diagnostic_ =
            "libvulkan.so.1 is not available";
        return;
    }

    get_instance_proc_addr_ =
        reinterpret_cast<
            GetInstanceProcAddr>(
                dlsym(
                    library_,
                    "vkGetInstanceProcAddr"));

    if (!get_instance_proc_addr_) {
        diagnostic_ =
            "vkGetInstanceProcAddr is missing from libvulkan.so.1";
        close();
        return;
    }

    diagnostic_ =
        "Vulkan loader available";
}

VulkanLoader::~VulkanLoader() {
    close();
}

VulkanLoader::VulkanLoader(
    VulkanLoader&& other) noexcept
    : library_(
          std::exchange(
              other.library_,
              nullptr)),
      get_instance_proc_addr_(
          std::exchange(
              other.get_instance_proc_addr_,
              nullptr)),
      diagnostic_(
          std::move(
              other.diagnostic_)) {}

VulkanLoader& VulkanLoader::operator=(
    VulkanLoader&& other) noexcept {

    if (this == &other) {
        return *this;
    }

    close();

    library_ =
        std::exchange(
            other.library_,
            nullptr);

    get_instance_proc_addr_ =
        std::exchange(
            other.get_instance_proc_addr_,
            nullptr);

    diagnostic_ =
        std::move(
            other.diagnostic_);

    return *this;
}

void* VulkanLoader::get_proc_address(
    const char* name) const noexcept {

    if (!loaded() ||
        !name ||
        !*name) {
        return nullptr;
    }

    return get_instance_proc_addr_(
        nullptr,
        name);
}

void VulkanLoader::close() noexcept {
    if (library_) {
        dlclose(library_);
    }

    library_ = nullptr;
    get_instance_proc_addr_ =
        nullptr;
}

} // namespace nengine::render
