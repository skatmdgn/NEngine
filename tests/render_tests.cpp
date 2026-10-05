#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "nengine/assets/asset_guid.hpp"
#include "nengine/core/scene.hpp"
#include "nengine/core/world.hpp"
#include "nengine/render/asset_resources.hpp"
#include "nengine/render/components.hpp"
#include "nengine/render/mesh_data.hpp"
#include "nengine/render/registration.hpp"
#include "nengine/render/render_snapshot.hpp"
#include "nengine/render/rhi.hpp"
#include "nengine/render/vulkan_loader.hpp"
#include "nengine/render/vulkan_device.hpp"
#include "nengine/render/vulkan_instance.hpp"

namespace {
int failures = 0;

void check(
    bool condition,
    const char* message) {

    if (!condition) {
        ++failures;
        std::cerr
            << "FAIL: "
            << message
            << '\n';
    }
}
} // namespace

int main() {
    using namespace nengine;

    core::ComponentRegistry metadata;
    core::ComponentSerializationRegistry serialization;

    check(
        render::register_component_metadata(
            metadata),
        "render component metadata registers");

    check(
        render::register_component_serializers(
            serialization),
        "render component serializers register");

    check(
        metadata.find(
            render::camera_type()) != nullptr &&
        metadata.find(
            render::light_type()) != nullptr &&
        metadata.find(
            render::mesh_renderer_type()) != nullptr,
        "render component descriptors are discoverable");

    core::World world;

    const auto camera_entity =
        world.create("Main Camera");

    auto* camera =
        world.add_component<render::Camera>(
            camera_entity,
            render::camera_type());

    check(
        camera != nullptr,
        "camera component attaches");

    if (camera) {
        camera->vertical_fov_degrees =
            72.0f;
        camera->near_clip =
            0.05f;
    }

    const auto light_entity =
        world.create("Key Light");

    auto* light =
        world.add_component<render::Light>(
            light_entity,
            render::light_type());

    check(
        light != nullptr,
        "light component attaches");

    if (light) {
        light->type =
            render::LightType::Point;
        light->intensity =
            3.5f;
        light->color =
            {1.0f, 0.8f, 0.6f};
    }

    const auto mesh_entity =
        world.create("Renderable");

    auto* mesh_renderer =
        world.add_component<
            render::MeshRenderer>(
                mesh_entity,
                render::mesh_renderer_type());

    check(
        mesh_renderer != nullptr,
        "mesh renderer component attaches");

    const auto mesh_guid =
        assets::AssetGuid::generate();

    const auto material_guid =
        assets::AssetGuid::generate();

    if (mesh_renderer) {
        mesh_renderer->mesh =
            mesh_guid;
        mesh_renderer->material =
            material_guid;
        mesh_renderer->receive_shadows =
            false;
    }

    const auto hidden_entity =
        world.create("Hidden");

    world.add_component<render::MeshRenderer>(
        hidden_entity,
        render::mesh_renderer_type());

    world.set_active(
        hidden_entity,
        false);

    const auto snapshot =
        render::build_render_snapshot(
            world);

    check(
        snapshot.cameras.size() == 1,
        "render snapshot finds enabled active camera");

    check(
        snapshot.lights.size() == 1,
        "render snapshot finds enabled active light");

    check(
        snapshot.meshes.size() == 1,
        "render snapshot excludes inactive mesh entity");

    check(
        snapshot.meshes.size() == 1 &&
        snapshot.meshes[0].renderer.mesh ==
            mesh_guid &&
        snapshot.meshes[0].renderer.material ==
            material_guid,
        "render snapshot preserves asset GUID references");

    const auto scene =
        core::SceneSerializer::capture(
            world,
            "RenderTest",
            &serialization);

    std::stringstream stream;
    std::string error;

    check(
        core::SceneSerializer::write(
            scene,
            stream,
            &error),
        "render components serialize into Scene v3");

    core::SceneData loaded;

    check(
        core::SceneSerializer::read(
            stream,
            loaded,
            &error),
        "render component scene parses");

    core::World restored;

    check(
        core::SceneSerializer::instantiate(
            loaded,
            restored,
            &error,
            &serialization),
        "render component scene restores");

    core::Entity restored_camera =
        core::Entity::invalid();

    core::Entity restored_mesh =
        core::Entity::invalid();

    for (const auto entity :
         restored.entities()) {

        if (restored.name(entity) ==
            "Main Camera") {
            restored_camera =
                entity;
        }

        if (restored.name(entity) ==
            "Renderable") {
            restored_mesh =
                entity;
        }
    }

    const auto* restored_camera_component =
        restored_camera.valid()
            ? restored.get_component<
                render::Camera>(
                    restored_camera,
                    render::camera_type())
            : nullptr;

    check(
        restored_camera_component &&
        restored_camera_component
            ->vertical_fov_degrees ==
            72.0f,
        "camera properties survive Scene roundtrip");

    const auto* restored_mesh_component =
        restored_mesh.valid()
            ? restored.get_component<
                render::MeshRenderer>(
                    restored_mesh,
                    render::mesh_renderer_type())
            : nullptr;

    check(
        restored_mesh_component &&
        restored_mesh_component->mesh ==
            mesh_guid &&
        restored_mesh_component->material ==
            material_guid &&
        !restored_mesh_component
            ->receive_shadows,
        "MeshRenderer AssetReferences survive Scene roundtrip");

    {
        std::stringstream texture_descriptor;
        texture_descriptor
            << "NENGINE_TEXTURE 1\n"
            << "FORMAT \"png\"\n"
            << "WIDTH 64\n"
            << "HEIGHT 32\n"
            << "COLOR_SPACE \"sRGB\"\n"
            << "SOURCE \"source.png\"\n"
            << "END_TEXTURE\n";

        render::TextureAssetMetadata metadata;
        std::string metadata_error;

        check(
            render::read_texture_asset_metadata(
                texture_descriptor,
                metadata,
                &metadata_error) &&
            metadata.width == 64 &&
            metadata.height == 32 &&
            metadata.format == "png",
            "renderer parses imported texture metadata");

        std::stringstream model_descriptor;
        model_descriptor
            << "NENGINE_MODEL 1\n"
            << "FORMAT \".gltf\"\n"
            << "SOURCE \"source.gltf\"\n"
            << "SOURCE_BYTES 1234\n"
            << "END_MODEL\n";

        render::ModelAssetMetadata model_metadata;

        check(
            render::read_model_asset_metadata(
                model_descriptor,
                model_metadata,
                &metadata_error) &&
            model_metadata.source_bytes == 1234 &&
            model_metadata.format == ".gltf",
            "renderer parses imported model metadata");
    }

    {
        const auto stamp =
            std::chrono::high_resolution_clock::now()
                .time_since_epoch()
                .count();

        const auto root =
            std::filesystem::temp_directory_path() /
            ("nengine_render_asset_" +
             std::to_string(stamp));

        std::filesystem::create_directories(root);

        const auto source =
            root / "source.png";

        const auto descriptor =
            root / "texture.nasset";

        {
            std::ofstream output(
                source,
                std::ios::binary |
                    std::ios::trunc);

            output << "source";
        }

        {
            std::ofstream output(
                descriptor,
                std::ios::binary |
                    std::ios::trunc);

            output
                << "NENGINE_TEXTURE 1\n"
                << "FORMAT \"png\"\n"
                << "WIDTH 128\n"
                << "HEIGHT 96\n"
                << "COLOR_SPACE \"sRGB\"\n"
                << "SOURCE \"source.png\"\n"
                << "END_TEXTURE\n";
        }

        assets::CachedArtifactSet cached;
        cached.importer_id =
            "NEngine.Texture";

        cached.artifacts.push_back({
            source,
            "source"
        });

        cached.artifacts.push_back({
            descriptor,
            "texture-descriptor"
        });

        const auto texture_guid =
            assets::AssetGuid::generate();

        std::string resolve_error;

        const auto resolved =
            render::resolve_texture_asset(
                texture_guid,
                cached,
                &resolve_error);

        check(
            resolved.has_value() &&
            resolved->guid == texture_guid &&
            resolved->metadata.width == 128 &&
            resolved->metadata.height == 96 &&
            resolved->source_path == source,
            "renderer resolves texture GUID from validated cache artifacts");

        std::error_code cleanup_error;
        std::filesystem::remove_all(
            root,
            cleanup_error);
    }

    {
        render::VulkanLoader loader;

        check(
            !loader.diagnostic().empty(),
            "Vulkan loader always reports diagnostics");

        if (loader.loaded()) {
            check(
                loader.get_proc_address(
                    "vkCreateInstance") != nullptr,
                "loaded Vulkan loader resolves vkCreateInstance");

            std::string extension_error;

            const auto extensions =
                render::VulkanInstance::
                    enumerate_extensions(
                        loader,
                        &extension_error);

            check(
                extension_error.empty(),
                "Vulkan loader can enumerate instance extensions");

            render::VulkanInstance rejected;

            check(
                !rejected.create(
                    loader,
                    "NEngineRenderTests",
                    {
                        "VK_NENGINE_extension_that_does_not_exist"
                    }),
                "Vulkan bootstrap rejects missing required extension");

            check(
                !rejected.diagnostic().empty(),
                "failed Vulkan bootstrap reports diagnostic");

            render::VulkanInstance instance;

            if (instance.create(
                    loader,
                    "NEngineRenderTests")) {

                check(
                    instance.valid(),
                    "Vulkan instance is valid after successful creation");

                render::VulkanDevice device;

                if (device.create(
                        loader,
                        instance)) {

                    check(
                        device.valid() &&
                        device.physical_device() != nullptr &&
                        device.graphics_queue() != nullptr &&
                        device.graphics_queue_family() !=
                            0xFFFFFFFFu,
                        "Vulkan logical device exposes graphics queue state");

                    device.destroy();

                    check(
                        !device.valid(),
                        "Vulkan device destroy clears native handle");
                } else {
                    check(
                        !device.diagnostic().empty(),
                        "Vulkan device bootstrap failure is diagnostic rather than fatal");
                }

                instance.destroy();

                check(
                    !instance.valid(),
                    "Vulkan instance destroy clears native handle");
            } else {
                check(
                    !instance.diagnostic().empty(),
                    "Vulkan instance failure is diagnostic rather than fatal");
            }

            (void)extensions;
        }
    }

    {
        const auto cube =
            render::make_unit_cube_mesh();

        check(
            cube.valid() &&
            cube.vertices.size() == 24 &&
            cube.indices.size() == 36,
            "built-in cube mesh has expected face-split topology");

        bool cube_indices_valid = true;

        for (const auto index :
             cube.indices) {

            if (index >=
                cube.vertices.size()) {
                cube_indices_valid = false;
                break;
            }
        }

        check(
            cube_indices_valid &&
            cube.bounds.extents ==
                core::Vec3{
                    0.5f,
                    0.5f,
                    0.5f},
            "built-in cube indices and bounds are valid");

        const auto quad =
            render::make_unit_quad_mesh();

        check(
            quad.valid() &&
            quad.vertices.size() == 4 &&
            quad.indices.size() == 6 &&
            quad.bounds.extents ==
                core::Vec3{
                    0.5f,
                    0.5f,
                    0.0f},
            "built-in quad mesh topology and bounds are valid");
    }

    render::BufferHandle invalid_buffer;
    check(
        !invalid_buffer.valid(),
        "default RHI buffer handle is invalid");

    render::TextureDesc texture_desc;
    check(
        texture_desc.width == 1 &&
        texture_desc.height == 1 &&
        texture_desc.mip_levels == 1,
        "RHI texture descriptor has safe defaults");

    if (failures == 0) {
        std::cout
            << "NEngineRenderTests: PASS\n";
        return EXIT_SUCCESS;
    }

    std::cerr
        << "NEngineRenderTests: "
        << failures
        << " failure(s)\n";

    return EXIT_FAILURE;
}
