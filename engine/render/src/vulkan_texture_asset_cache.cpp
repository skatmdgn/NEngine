#include "nengine/render/vulkan_texture_asset_cache.hpp"

#include <utility>

namespace nengine::render {
namespace {

void set_error(
    std::string* error,
    std::string message) {

    if (error) {
        *error = std::move(message);
    }
}

} // namespace

VulkanTextureAssetCache::~VulkanTextureAssetCache() {
    shutdown();
}

bool VulkanTextureAssetCache::initialize(
    const VulkanLoader& loader,
    const VulkanInstance& instance,
    const VulkanDevice& device) {

    shutdown();
    diagnostic_.clear();

    if (!loader.loaded() ||
        !instance.valid() ||
        !device.valid()) {

        diagnostic_ =
            "valid Vulkan loader instance and device are required for texture asset cache";
        return false;
    }

    loader_ = &loader;
    instance_ = &instance;
    device_ = &device;

    diagnostic_ =
        "Vulkan texture asset cache ready";

    return true;
}

const VulkanTextureAssetResource*
VulkanTextureAssetCache::load(
    assets::AssetGuid guid,
    const assets::CachedArtifactSet& artifacts,
    std::string* error) {

    if (!ready()) {
        diagnostic_ =
            "Vulkan texture asset cache is not initialized";
        set_error(
            error,
            diagnostic_);
        return nullptr;
    }

    const auto existing =
        entries_.find(guid);

    if (existing != entries_.end() &&
        existing->second.fingerprint ==
            artifacts.fingerprint &&
        existing->second.resource.valid()) {

        return &existing->second.resource;
    }

    const auto* decoded =
        decoded_cache_.load(
            guid,
            artifacts,
            error);

    if (!decoded) {
        diagnostic_ =
            error && !error->empty()
                ? *error
                : "texture asset decode failed";
        return nullptr;
    }

    return upload(
        guid,
        artifacts.fingerprint,
        *decoded,
        error);
}

const VulkanTextureAssetResource*
VulkanTextureAssetCache::upload(
    assets::AssetGuid guid,
    std::string fingerprint,
    const DecodedTextureData& decoded,
    std::string* error) {

    if (!ready()) {
        diagnostic_ =
            "Vulkan texture asset cache is not initialized";
        set_error(
            error,
            diagnostic_);
        return nullptr;
    }

    if (!guid.valid() ||
        !decoded.valid()) {

        diagnostic_ =
            "valid AssetGuid and decoded RGBA8 pixels are required";
        set_error(
            error,
            diagnostic_);
        return nullptr;
    }

    const auto existing =
        entries_.find(guid);

    if (existing != entries_.end() &&
        existing->second.fingerprint ==
            fingerprint &&
        existing->second.resource.valid()) {

        return &existing->second.resource;
    }

    if (existing != entries_.end()) {
        entries_.erase(existing);
    }

    Entry entry;
    entry.fingerprint =
        std::move(fingerprint);

    const auto color_space =
        decoded.color_space ==
            DecodedTextureColorSpace::SRgb
            ? VulkanTextureColorSpace::SRgb
            : VulkanTextureColorSpace::Linear;

    if (!entry.resource.texture.create_rgba8(
            *loader_,
            *instance_,
            *device_,
            decoded.width,
            decoded.height,
            decoded.rgba8.data(),
            decoded.rgba8.size(),
            color_space)) {

        diagnostic_ =
            "Vulkan texture asset upload failed: " +
            entry.resource.texture.diagnostic();

        set_error(
            error,
            diagnostic_);
        return nullptr;
    }

    if (!entry.resource.material.create_textured(
            *device_,
            entry.resource.texture)) {

        diagnostic_ =
            "Vulkan texture asset material creation failed: " +
            entry.resource.material.diagnostic();

        set_error(
            error,
            diagnostic_);
        return nullptr;
    }

    const auto [it, inserted] =
        entries_.emplace(
            guid,
            std::move(entry));

    if (!inserted) {
        diagnostic_ =
            "Vulkan texture asset cache insertion failed";
        set_error(
            error,
            diagnostic_);
        return nullptr;
    }

    diagnostic_ =
        "Vulkan texture asset uploaded and cached";

    return &it->second.resource;
}

const VulkanTextureAssetResource*
VulkanTextureAssetCache::find(
    assets::AssetGuid guid) const noexcept {

    const auto it =
        entries_.find(guid);

    return
        it != entries_.end() &&
        it->second.resource.valid()
        ? &it->second.resource
        : nullptr;
}

bool VulkanTextureAssetCache::erase(
    assets::AssetGuid guid) noexcept {

    decoded_cache_.erase(guid);
    return entries_.erase(guid) != 0u;
}

void VulkanTextureAssetCache::shutdown() noexcept {
    entries_.clear();
    decoded_cache_.clear();

    loader_ = nullptr;
    instance_ = nullptr;
    device_ = nullptr;
}

} // namespace nengine::render
