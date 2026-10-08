#include "nengine/render/vulkan_diagnostic_scene.hpp"

#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include "nengine/core/transform.hpp"
#include "nengine/render/builtin_assets.hpp"
#include "nengine/render/diagnostic_shaders.hpp"
#include "nengine/render/matrix.hpp"
#include "nengine/render/model_importer.hpp"
#include "nengine/render/render_snapshot.hpp"

namespace nengine::render {

VulkanDiagnosticScene::~VulkanDiagnosticScene() {
    shutdown();
}

bool VulkanDiagnosticScene::initialize(
    VulkanContext& context) {

    shutdown();
    diagnostic_.clear();

    if (!context.ready()) {
        diagnostic_ =
            "Vulkan context is not ready";
        return false;
    }

    if (!vertex_shader_.create(
            context.device(),
            VulkanShaderStage::Vertex,
            diagnostic_textured_vertex_spirv())) {

        diagnostic_ =
            "diagnostic vertex shader failed: " +
            vertex_shader_.diagnostic();

        shutdown();
        return false;
    }

    if (!fragment_shader_.create(
            context.device(),
            VulkanShaderStage::Fragment,
            diagnostic_textured_fragment_spirv())) {

        diagnostic_ =
            "diagnostic fragment shader failed: " +
            fragment_shader_.diagnostic();

        shutdown();
        return false;
    }

    if (!mesh_cache_.initialize(
            context.loader(),
            context.instance(),
            context.device())) {

        diagnostic_ =
            "built-in Vulkan mesh cache failed: " +
            mesh_cache_.diagnostic();

        shutdown();
        return false;
    }

    if (!imported_mesh_cache_.initialize(
            context.loader(),
            context.instance(),
            context.device())) {

        diagnostic_ =
            "imported Vulkan mesh cache failed: " +
            imported_mesh_cache_.diagnostic();

        shutdown();
        return false;
    }

    if (!imported_material_cache_.initialize(
            context.loader(),
            context.instance(),
            context.device())) {

        diagnostic_ =
            "imported Vulkan material cache failed: " +
            imported_material_cache_.diagnostic();

        shutdown();
        return false;
    }

    const std::uint8_t pixels[] = {
        255u,  48u,  48u, 255u,
         48u, 255u,  96u, 255u,
         48u,  96u, 255u, 255u,
        255u, 224u,  48u, 255u
    };

    if (!texture_.create_rgba8(
            context.loader(),
            context.instance(),
            context.device(),
            2u,
            2u,
            pixels,
            sizeof(pixels),
            VulkanTextureColorSpace::SRgb)) {

        diagnostic_ =
            "diagnostic sampled texture failed: " +
            texture_.diagnostic();

        shutdown();
        return false;
    }

    if (!material_.create_textured(
            context.device(),
            texture_)) {

        diagnostic_ =
            "diagnostic textured material failed: " +
            material_.diagnostic();

        shutdown();
        return false;
    }

    if (!pipeline_.create(
            context.device(),
            context.render_targets()
                .render_pass_resource(),
            vertex_shader_,
            fragment_shader_,
            material_)) {

        diagnostic_ =
            "diagnostic textured graphics pipeline failed: " +
            pipeline_.diagnostic();

        shutdown();
        return false;
    }

    diagnostic_ =
        "Vulkan textured preview shaders mesh cache texture material and pipeline ready";

    return true;
}

bool VulkanDiagnosticScene::present(
    VulkanContext& context) {

    if (!ready() ||
        !context.ready()) {

        diagnostic_ =
            "Vulkan diagnostic scene is not ready";
        return false;
    }

    const auto height =
        context.swapchain().height();

    if (height == 0) {
        diagnostic_ =
            "Vulkan diagnostic scene cannot present to zero-height swapchain";
        return false;
    }

    const auto* quad =
        mesh_cache_.find(
            builtin_unit_quad_mesh_guid());

    if (!quad) {
        diagnostic_ =
            "built-in diagnostic quad is unavailable";
        return false;
    }

    const float aspect =
        static_cast<float>(
            context.swapchain().width()) /
        static_cast<float>(
            height);

    core::Transform model_transform;
    model_transform.local_position =
        {0.0f, 0.0f, 2.0f};

    const auto model =
        transform_matrix(
            model_transform);

    const auto projection =
        perspective_lh_zo(
            60.0f,
            aspect,
            0.1f,
            100.0f);

    const auto mvp =
        multiply(
            projection,
            model);

    const VulkanMeshDraw draw{
        &pipeline_,
        quad,
        mvp,
        &material_
    };

    if (!context.present_meshes(
            std::span<const VulkanMeshDraw>{
                &draw,
                1})) {

        diagnostic_ =
            context.diagnostic();
        return false;
    }

    diagnostic_ =
        "Vulkan diagnostic textured quad presented";

    return true;
}

bool VulkanDiagnosticScene::present_world(
    VulkanContext& context,
    const core::World& world,
    const CachedArtifactResolver&
        asset_resolver) {

    if (!ready() ||
        !context.ready()) {

        diagnostic_ =
            "Vulkan World preview is not ready";
        return false;
    }

    const auto height =
        context.swapchain().height();

    if (height == 0) {
        diagnostic_ =
            "Vulkan World preview cannot present to zero-height swapchain";
        return false;
    }

    const float aspect =
        static_cast<float>(
            context.swapchain().width()) /
        static_cast<float>(
            height);

    const auto snapshot =
        build_render_snapshot(
            world);

    if (snapshot.cameras.empty()) {
        diagnostic_ =
            "World contains no active Camera; using diagnostic fallback";

        return present(context);
    }

    const auto matrices =
        build_camera_matrices(
            world,
            snapshot.cameras.front().entity,
            aspect);

    if (!matrices) {
        diagnostic_ =
            "active Camera matrices could not be built";
        return false;
    }

    std::vector<VulkanMeshDraw> draws;
    draws.reserve(
        snapshot.meshes.size());

    std::size_t imported_draws = 0;
    std::size_t imported_materials = 0;
    std::size_t cooked_materials = 0;
    std::size_t gltf_auto_materials = 0;
    std::size_t unresolved_draws = 0;
    std::size_t unresolved_materials = 0;
    std::string last_asset_error;

    for (const auto& item :
         snapshot.meshes) {

        const VulkanMeshResource* mesh =
            mesh_cache_.find(
                item.renderer.mesh);

        bool imported = false;

        if (!mesh) {
            mesh =
                imported_mesh_cache_.find(
                    item.renderer.mesh);

            imported =
                mesh != nullptr;
        }

        if (!mesh &&
            asset_resolver) {

            const auto artifacts =
                asset_resolver(
                    item.renderer.mesh);

            if (artifacts) {
                std::string asset_error;

                mesh =
                    imported_mesh_cache_.load(
                        item.renderer.mesh,
                        *artifacts,
                        &asset_error);

                imported =
                    mesh != nullptr;

                if (!mesh &&
                    !asset_error.empty()) {
                    last_asset_error =
                        std::move(
                            asset_error);
                }
            }
        }

        if (!mesh) {
            ++unresolved_draws;
            continue;
        }

        if (imported) {
            ++imported_draws;
        }

        const bool explicit_material_requested =
            item.renderer.material.valid();

        const VulkanMaterialResource*
            explicit_material = nullptr;

        if (explicit_material_requested) {
            explicit_material =
                imported_material_cache_.find(
                    item.renderer.material);

            if (!explicit_material &&
                asset_resolver) {

                const auto material_artifacts =
                    asset_resolver(
                        item.renderer.material);

                if (material_artifacts) {
                    std::string material_error;

                    explicit_material =
                        imported_material_cache_.load(
                            item.renderer.material,
                            *material_artifacts,
                            asset_resolver,
                            &material_error);

                    if (!explicit_material &&
                        !material_error.empty()) {
                        last_asset_error =
                            std::move(
                                material_error);
                    }
                }
            }

            if (explicit_material) {
                ++imported_materials;
            } else {
                ++unresolved_materials;
            }
        }

        std::optional<
            assets::CachedArtifactSet>
            model_artifacts;

        const CookedModelMaterialMap*
            cooked_material_map = nullptr;

        if (!explicit_material_requested &&
            imported &&
            asset_resolver) {

            model_artifacts =
                asset_resolver(
                    item.renderer.mesh);

            if (model_artifacts) {
                auto& cached_map =
                    cooked_model_material_maps_[
                        item.renderer.mesh];

                if (cached_map.fingerprint !=
                        model_artifacts
                            ->fingerprint) {

                    cached_map = {};

                    if (read_cooked_model_material_map(
                            *model_artifacts,
                            cached_map.materials)) {

                        cached_map.fingerprint =
                            model_artifacts
                                ->fingerprint;
                    }
                }

                if (cached_map.fingerprint ==
                    model_artifacts
                        ->fingerprint) {

                    cooked_material_map =
                        &cached_map.materials;
                }
            }
        }

        const auto mvp =
            multiply(
                matrices->view_projection,
                item.world);

        for (const auto& submesh :
             mesh->submeshes()) {

            const VulkanMaterialResource*
                draw_material =
                    explicit_material
                        ? explicit_material
                        : &material_;

            if (!explicit_material_requested &&
                imported &&
                submesh.material_slot !=
                    kMeshMaterialUnassigned) {

                const VulkanMaterialResource*
                    automatic = nullptr;

                bool used_cooked =
                    false;

                if (cooked_material_map &&
                    asset_resolver) {

                    const auto cooked =
                        cooked_material_map
                            ->find(
                                submesh.material_slot);

                    if (cooked !=
                        cooked_material_map
                            ->end()) {

                        const auto cooked_guid =
                            cooked->second;
                        automatic =
                            imported_material_cache_
                                .find(
                                    cooked_guid);

                        if (!automatic) {
                            const auto cooked_artifacts =
                                asset_resolver(
                                    cooked_guid);

                            if (cooked_artifacts) {
                                std::string
                                    material_error;

                                automatic =
                                    imported_material_cache_
                                        .load(
                                            cooked_guid,
                                            *cooked_artifacts,
                                            asset_resolver,
                                            &material_error);

                                if (!automatic &&
                                    !material_error.empty()) {

                                    last_asset_error =
                                        std::move(
                                            material_error);
                                }
                            }
                        }

                        used_cooked =
                            automatic != nullptr;
                    }
                }

                if (!automatic) {
                    automatic =
                        imported_material_cache_
                            .find_gltf_material(
                                item.renderer.mesh,
                                submesh.material_slot);
                }

                if (!automatic &&
                    model_artifacts) {

                    std::string
                        material_error;

                    automatic =
                        imported_material_cache_
                            .load_gltf_material(
                                item.renderer.mesh,
                                submesh.material_slot,
                                *model_artifacts,
                                &material_error);

                    if (!automatic &&
                        !material_error.empty()) {

                        last_asset_error =
                            std::move(
                                material_error);
                    }
                }

                if (automatic) {
                    draw_material =
                        automatic;

                    if (used_cooked) {
                        ++cooked_materials;
                    } else {
                        ++gltf_auto_materials;
                    }
                } else {
                    ++unresolved_materials;
                }
            }

            draws.push_back({
                &pipeline_,
                mesh,
                mvp,
                draw_material,
                submesh.first_index,
                submesh.index_count
            });
        }
    }

    if (draws.empty()) {
        if (!context.present_clear(
                0.08f,
                0.09f,
                0.11f,
                1.0f)) {

            diagnostic_ =
                context.diagnostic();
            return false;
        }

        diagnostic_ =
            "Vulkan World preview cleared; no resolvable MeshRenderer items found";

        if (unresolved_draws != 0u &&
            !last_asset_error.empty()) {
            diagnostic_ +=
                ": " +
                last_asset_error;
        }

        return true;
    }

    if (!context.present_meshes(
            draws)) {

        diagnostic_ =
            context.diagnostic();
        return false;
    }

    diagnostic_ =
        "Vulkan World preview presented " +
        std::to_string(
            draws.size()) +
        " draw(s), " +
        std::to_string(
            imported_draws) +
        " imported mesh, " +
        std::to_string(
            imported_materials) +
        " imported material, " +
        std::to_string(cooked_materials) +
        " cooked glTF material, " +
        std::to_string(gltf_auto_materials) +
        " direct glTF fallback";

    if (unresolved_draws != 0u) {
        diagnostic_ +=
            ", " +
            std::to_string(
                unresolved_draws) +
            " unresolved mesh";
    }

    if (unresolved_materials != 0u) {
        diagnostic_ +=
            ", " +
            std::to_string(
                unresolved_materials) +
            " unresolved material";
    }

    return true;
}

void VulkanDiagnosticScene::shutdown() noexcept {
    pipeline_.destroy();
    material_.destroy();
    texture_.destroy();
    imported_material_cache_.shutdown();
    imported_mesh_cache_.shutdown();
    cooked_model_material_maps_.clear();
    mesh_cache_.shutdown();
    fragment_shader_.destroy();
    vertex_shader_.destroy();
}

} // namespace nengine::render
