#include "nengine/render/vulkan_mesh_asset_cache.hpp"

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

VulkanMeshAssetCache::~VulkanMeshAssetCache() {
    shutdown();
}

bool VulkanMeshAssetCache::initialize(
    const VulkanLoader& loader,
    const VulkanInstance& instance,
    const VulkanDevice& device) {

    shutdown();
    diagnostic_.clear();

    if (!loader.loaded() ||
        !instance.valid() ||
        !device.valid()) {

        diagnostic_ =
            "valid Vulkan loader instance and device are required for mesh asset cache";
        return false;
    }

    loader_ = &loader;
    instance_ = &instance;
    device_ = &device;

    diagnostic_ =
        "Vulkan mesh asset cache ready";

    return true;
}

const VulkanMeshResource*
VulkanMeshAssetCache::load(
    assets::AssetGuid guid,
    const assets::CachedArtifactSet& artifacts,
    std::string* error) {

    if (!ready()) {
        diagnostic_ =
            "Vulkan mesh asset cache is not initialized";
        set_error(
            error,
            diagnostic_);
        return nullptr;
    }

    const auto existing =
        entries_.find(guid);

    if (existing !=
            entries_.end() &&
        existing->second.fingerprint ==
            artifacts.fingerprint &&
        existing->second.mesh.valid()) {

        return &existing->second.mesh;
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
                : "mesh asset decode failed";
        return nullptr;
    }

    return upload(
        guid,
        artifacts.fingerprint,
        *decoded,
        error);
}

const VulkanMeshResource*
VulkanMeshAssetCache::upload(
    assets::AssetGuid guid,
    std::string fingerprint,
    const MeshData& mesh,
    std::string* error) {

    if (!ready()) {
        diagnostic_ =
            "Vulkan mesh asset cache is not initialized";
        set_error(
            error,
            diagnostic_);
        return nullptr;
    }

    if (!guid.valid() ||
        !mesh.valid()) {

        diagnostic_ =
            "valid AssetGuid and MeshData are required";
        set_error(
            error,
            diagnostic_);
        return nullptr;
    }

    const auto existing =
        entries_.find(guid);

    if (existing !=
            entries_.end() &&
        existing->second.fingerprint ==
            fingerprint &&
        existing->second.mesh.valid()) {

        return &existing->second.mesh;
    }

    if (existing !=
        entries_.end()) {
        entries_.erase(existing);
    }

    Entry entry;
    entry.fingerprint =
        std::move(fingerprint);

    if (!entry.mesh.create(
            *loader_,
            *instance_,
            *device_,
            mesh)) {

        diagnostic_ =
            "Vulkan mesh asset upload failed: " +
            entry.mesh.diagnostic();

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
            "Vulkan mesh asset cache insertion failed";
        set_error(
            error,
            diagnostic_);
        return nullptr;
    }

    diagnostic_ =
        "Vulkan mesh asset uploaded and cached";

    return &it->second.mesh;
}

const VulkanMeshResource*
VulkanMeshAssetCache::find(
    assets::AssetGuid guid) const noexcept {

    const auto it =
        entries_.find(guid);

    return
        it != entries_.end() &&
        it->second.mesh.valid()
        ? &it->second.mesh
        : nullptr;
}

bool VulkanMeshAssetCache::erase(
    assets::AssetGuid guid) noexcept {

    decoded_cache_.erase(guid);
    return entries_.erase(guid) != 0u;
}

void VulkanMeshAssetCache::shutdown() noexcept {
    entries_.clear();
    decoded_cache_.clear();

    loader_ = nullptr;
    instance_ = nullptr;
    device_ = nullptr;
}

} // namespace nengine::render
