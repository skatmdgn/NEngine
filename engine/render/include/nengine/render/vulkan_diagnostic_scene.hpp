#pragma once

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>

#include "nengine/assets/import_pipeline.hpp"
#include "nengine/core/world.hpp"
#include "nengine/render/vulkan_builtin_mesh_cache.hpp"
#include "nengine/render/vulkan_context.hpp"
#include "nengine/render/vulkan_material.hpp"
#include "nengine/render/model_importer.hpp"
#include "nengine/render/vulkan_material_asset_cache.hpp"
#include "nengine/render/vulkan_mesh_asset_cache.hpp"
#include "nengine/render/vulkan_pipeline.hpp"
#include "nengine/render/vulkan_shader.hpp"
#include "nengine/render/vulkan_texture.hpp"

namespace nengine::render {

using CachedArtifactResolver =
    std::function<
        std::optional<assets::CachedArtifactSet>(
            assets::AssetGuid)>;

class VulkanDiagnosticScene {
public:
    VulkanDiagnosticScene() = default;
    ~VulkanDiagnosticScene();

    VulkanDiagnosticScene(
        const VulkanDiagnosticScene&) = delete;

    VulkanDiagnosticScene& operator=(
        const VulkanDiagnosticScene&) = delete;

    bool initialize(
        VulkanContext& context);

    // Fixed diagnostic quad fallback.
    bool present(
        VulkanContext& context);

    // Render supported MeshRenderer items from an actual World.
    bool present_world(
        VulkanContext& context,
        const core::World& world,
        const CachedArtifactResolver&
            asset_resolver = {});

    void invalidate_imported_assets() noexcept {
        imported_mesh_cache_.clear();
        imported_material_cache_.clear();
        cooked_model_material_maps_.clear();
    }

    void shutdown() noexcept;

    bool ready() const noexcept {
        return vertex_shader_.valid() &&
            fragment_shader_.valid() &&
            mesh_cache_.ready() &&
            imported_mesh_cache_.ready() &&
            imported_material_cache_.ready() &&
            texture_.valid() &&
            material_.valid() &&
            pipeline_.valid() &&
            sprite_pipeline_.valid();
    }

    const std::string& diagnostic() const noexcept {
        return diagnostic_;
    }

private:
    struct CookedMaterialMapCacheEntry {
        std::string fingerprint{};
        CookedModelMaterialMap materials{};
    };

    VulkanShaderModule vertex_shader_{};
    VulkanShaderModule fragment_shader_{};
    VulkanBuiltinMeshCache mesh_cache_{};
    VulkanMeshAssetCache imported_mesh_cache_{};
    VulkanMaterialAssetCache imported_material_cache_{};

    std::unordered_map<
        assets::AssetGuid,
        CookedMaterialMapCacheEntry,
        assets::AssetGuidHash>
        cooked_model_material_maps_{};

    VulkanTextureResource texture_{};
    VulkanMaterialResource material_{};
    VulkanGraphicsPipeline pipeline_{};
    VulkanGraphicsPipeline sprite_pipeline_{};
    std::string diagnostic_{};
};

} // namespace nengine::render
