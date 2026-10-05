#include "nengine/render/vulkan_instance.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

namespace nengine::render {
namespace {

using VkResult = std::int32_t;

constexpr VkResult VK_SUCCESS = 0;
constexpr VkResult VK_INCOMPLETE = 5;

constexpr std::uint32_t
VK_STRUCTURE_TYPE_APPLICATION_INFO = 0;

constexpr std::uint32_t
VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO = 1;

constexpr std::uint32_t make_api_version(
    std::uint32_t major,
    std::uint32_t minor,
    std::uint32_t patch) noexcept {

    return
        (major << 22u) |
        (minor << 12u) |
        patch;
}

struct VkApplicationInfo {
    std::uint32_t sType;
    const void* pNext;
    const char* pApplicationName;
    std::uint32_t applicationVersion;
    const char* pEngineName;
    std::uint32_t engineVersion;
    std::uint32_t apiVersion;
};

struct VkInstanceCreateInfo {
    std::uint32_t sType;
    const void* pNext;
    std::uint32_t flags;
    const VkApplicationInfo* pApplicationInfo;
    std::uint32_t enabledLayerCount;
    const char* const* ppEnabledLayerNames;
    std::uint32_t enabledExtensionCount;
    const char* const* ppEnabledExtensionNames;
};

struct VkExtensionProperties {
    char extensionName[256];
    std::uint32_t specVersion;
};

using CreateInstance =
    VkResult (*)(
        const VkInstanceCreateInfo*,
        const void*,
        void**);

using EnumerateInstanceExtensionProperties =
    VkResult (*)(
        const char*,
        std::uint32_t*,
        VkExtensionProperties*);

std::string result_message(
    std::string_view operation,
    VkResult result) {

    return
        std::string{operation} +
        " failed with VkResult " +
        std::to_string(result);
}

bool contains_extension(
    const std::vector<VulkanExtensionInfo>&
        available,
    std::string_view name) {

    return std::any_of(
        available.begin(),
        available.end(),
        [name](
            const VulkanExtensionInfo& extension) {
            return extension.name == name;
        });
}

} // namespace

VulkanInstance::~VulkanInstance() {
    destroy();
}

VulkanInstance::VulkanInstance(
    VulkanInstance&& other) noexcept
    : instance_(
          std::exchange(
              other.instance_,
              nullptr)),
      destroy_instance_(
          std::exchange(
              other.destroy_instance_,
              nullptr)),
      diagnostic_(
          std::move(
              other.diagnostic_)) {}

VulkanInstance& VulkanInstance::operator=(
    VulkanInstance&& other) noexcept {

    if (this == &other) {
        return *this;
    }

    destroy();

    instance_ =
        std::exchange(
            other.instance_,
            nullptr);

    destroy_instance_ =
        std::exchange(
            other.destroy_instance_,
            nullptr);

    diagnostic_ =
        std::move(
            other.diagnostic_);

    return *this;
}

std::vector<VulkanExtensionInfo>
VulkanInstance::enumerate_extensions(
    const VulkanLoader& loader,
    std::string* error) {

    std::vector<VulkanExtensionInfo> result;

    if (!loader.loaded()) {
        if (error) {
            *error =
                loader.diagnostic();
        }
        return result;
    }

    const auto function =
        reinterpret_cast<
            EnumerateInstanceExtensionProperties>(
                loader.get_proc_address(
                    "vkEnumerateInstanceExtensionProperties"));

    if (!function) {
        if (error) {
            *error =
                "vkEnumerateInstanceExtensionProperties is unavailable";
        }
        return result;
    }

    std::uint32_t count = 0;

    auto status =
        function(
            nullptr,
            &count,
            nullptr);

    if (status != VK_SUCCESS ||
        count == 0) {

        if (error && status != VK_SUCCESS) {
            *error =
                result_message(
                    "vkEnumerateInstanceExtensionProperties",
                    status);
        }

        return result;
    }

    std::vector<VkExtensionProperties>
        properties(count);

    status =
        function(
            nullptr,
            &count,
            properties.data());

    if (status != VK_SUCCESS &&
        status != VK_INCOMPLETE) {

        if (error) {
            *error =
                result_message(
                    "vkEnumerateInstanceExtensionProperties",
                    status);
        }

        return {};
    }

    properties.resize(count);
    result.reserve(count);

    for (const auto& property :
         properties) {

        VulkanExtensionInfo extension;
        extension.name =
            property.extensionName;
        extension.specification_version =
            property.specVersion;

        result.push_back(
            std::move(extension));
    }

    return result;
}

bool VulkanInstance::create(
    const VulkanLoader& loader,
    std::string_view application_name,
    const std::vector<std::string>&
        required_extensions) {

    destroy();
    diagnostic_.clear();

    if (!loader.loaded()) {
        diagnostic_ =
            loader.diagnostic();
        return false;
    }

    const auto create_instance =
        reinterpret_cast<CreateInstance>(
            loader.get_proc_address(
                "vkCreateInstance"));

    if (!create_instance) {
        diagnostic_ =
            "vkCreateInstance is unavailable";
        return false;
    }

    if (!required_extensions.empty()) {
        std::string extension_error;

        const auto available =
            enumerate_extensions(
                loader,
                &extension_error);

        if (!extension_error.empty()) {
            diagnostic_ =
                std::move(
                    extension_error);
            return false;
        }

        for (const auto& required :
             required_extensions) {

            if (!contains_extension(
                    available,
                    required)) {

                diagnostic_ =
                    "required Vulkan instance extension is unavailable: " +
                    required;
                return false;
            }
        }
    }

    const std::string app_name =
        application_name.empty()
            ? "NEngineApp"
            : std::string{
                application_name};

    const VkApplicationInfo application{
        VK_STRUCTURE_TYPE_APPLICATION_INFO,
        nullptr,
        app_name.c_str(),
        make_api_version(0, 1, 0),
        "NEngine",
        make_api_version(0, 3, 0),
        make_api_version(1, 0, 0)
    };

    std::vector<const char*>
        extension_names;

    extension_names.reserve(
        required_extensions.size());

    for (const auto& extension :
         required_extensions) {
        extension_names.push_back(
            extension.c_str());
    }

    const VkInstanceCreateInfo info{
        VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        nullptr,
        0,
        &application,
        0,
        nullptr,
        static_cast<std::uint32_t>(
            extension_names.size()),
        extension_names.empty()
            ? nullptr
            : extension_names.data()
    };

    void* instance = nullptr;

    const auto status =
        create_instance(
            &info,
            nullptr,
            &instance);

    if (status != VK_SUCCESS ||
        !instance) {

        diagnostic_ =
            result_message(
                "vkCreateInstance",
                status);
        return false;
    }

    const auto destroy_instance =
        reinterpret_cast<DestroyInstance>(
            loader.get_instance_proc_address(
                instance,
                "vkDestroyInstance"));

    if (!destroy_instance) {
        diagnostic_ =
            "vkDestroyInstance is unavailable for created instance";

        using FallbackDestroy =
            void (*)(void*, const void*);

        const auto fallback =
            reinterpret_cast<FallbackDestroy>(
                loader.get_proc_address(
                    "vkDestroyInstance"));

        if (fallback) {
            fallback(
                instance,
                nullptr);
        }

        return false;
    }

    instance_ = instance;
    destroy_instance_ =
        destroy_instance;

    diagnostic_ =
        "Vulkan instance created";

    return true;
}

void VulkanInstance::destroy() noexcept {
    if (instance_ &&
        destroy_instance_) {

        destroy_instance_(
            instance_,
            nullptr);
    }

    instance_ = nullptr;
    destroy_instance_ = nullptr;
}

} // namespace nengine::render
