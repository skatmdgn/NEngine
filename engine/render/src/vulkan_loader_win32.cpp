#include "nengine/render/vulkan_loader.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <utility>

namespace nengine::render {

VulkanLoader::VulkanLoader() {
    const auto module =
        LoadLibraryW(
            L"vulkan-1.dll");

    if (!module) {
        diagnostic_ =
            "vulkan-1.dll is not available";
        return;
    }

    library_ = module;

    get_instance_proc_addr_ =
        reinterpret_cast<
            GetInstanceProcAddr>(
                GetProcAddress(
                    module,
                    "vkGetInstanceProcAddr"));

    if (!get_instance_proc_addr_) {
        diagnostic_ =
            "vkGetInstanceProcAddr is missing from vulkan-1.dll";
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
        FreeLibrary(
            static_cast<HMODULE>(
                library_));
    }

    library_ = nullptr;
    get_instance_proc_addr_ =
        nullptr;
}

} // namespace nengine::render
