#include <array>
#include <chrono>
#include <cmath>
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
#include "nengine/render/builtin_assets.hpp"
#include "nengine/render/components.hpp"
#include "nengine/render/decoded_mesh.hpp"
#include "nengine/render/decoded_texture.hpp"
#include "nengine/render/diagnostic_shaders.hpp"
#include "nengine/render/gltf_mesh.hpp"
#include "nengine/render/matrix.hpp"
#include "nengine/render/mesh_data.hpp"
#include "nengine/render/registration.hpp"
#include "nengine/render/render_snapshot.hpp"
#include "nengine/render/rhi.hpp"
#include "nengine/render/vulkan_buffer.hpp"
#include "nengine/render/vulkan_loader.hpp"
#include "nengine/render/vulkan_material.hpp"
#include "nengine/render/vulkan_depth_target.hpp"
#include "nengine/render/vulkan_device.hpp"
#include "nengine/render/vulkan_instance.hpp"
#include "nengine/render/vulkan_mesh.hpp"
#include "nengine/render/vulkan_mesh_asset_cache.hpp"
#include "nengine/render/vulkan_pipeline.hpp"
#include "nengine/render/vulkan_presenter.hpp"
#include "nengine/render/vulkan_render_pass.hpp"
#include "nengine/render/vulkan_shader.hpp"
#include "nengine/render/vulkan_texture.hpp"
#include "nengine/render/vulkan_texture_asset_cache.hpp"

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

    render::MeshData decoded_gltf_fixture;

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

    const auto mesh_parent =
        world.create("Mesh Parent");

    world.transform(mesh_parent)
        ->local_position =
        {5.0f, 0.0f, 0.0f};

    const auto mesh_entity =
        world.create("Renderable");

    world.transform(mesh_entity)
        ->local_position =
        {2.0f, 0.0f, 0.0f};

    world.set_parent(
        mesh_entity,
        mesh_parent);

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

    if (snapshot.meshes.size() == 1) {
        const auto resolved_origin =
            render::transform_point(
                snapshot.meshes[0].world,
                {0.0f, 0.0f, 0.0f});

        check(
            resolved_origin ==
                core::Vec3{
                    7.0f,
                    0.0f,
                    0.0f},
            "render snapshot resolves parent hierarchy into world matrix");
    }

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

        const auto bmp_source =
            root / "decoded.bmp";

        const auto bmp_descriptor =
            root / "decoded_texture.nasset";

        std::array<std::uint8_t, 70>
            bmp_bytes{};

        bmp_bytes[0] = 'B';
        bmp_bytes[1] = 'M';
        bmp_bytes[2] = 70u;
        bmp_bytes[10] = 54u;
        bmp_bytes[14] = 40u;
        bmp_bytes[18] = 2u;
        bmp_bytes[22] = 2u;
        bmp_bytes[26] = 1u;
        bmp_bytes[28] = 24u;
        bmp_bytes[34] = 16u;

        // Bottom BMP row: blue, white.
        bmp_bytes[54] = 255u;
        bmp_bytes[55] = 0u;
        bmp_bytes[56] = 0u;
        bmp_bytes[57] = 255u;
        bmp_bytes[58] = 255u;
        bmp_bytes[59] = 255u;

        // Top BMP row: red, green.
        bmp_bytes[62] = 0u;
        bmp_bytes[63] = 0u;
        bmp_bytes[64] = 255u;
        bmp_bytes[65] = 0u;
        bmp_bytes[66] = 255u;
        bmp_bytes[67] = 0u;

        {
            std::ofstream output(
                bmp_source,
                std::ios::binary |
                    std::ios::trunc);

            output.write(
                reinterpret_cast<const char*>(
                    bmp_bytes.data()),
                static_cast<std::streamsize>(
                    bmp_bytes.size()));
        }

        {
            std::ofstream output(
                bmp_descriptor,
                std::ios::binary |
                    std::ios::trunc);

            output
                << "NENGINE_TEXTURE 1\n"
                << "FORMAT \"bmp\"\n"
                << "WIDTH 2\n"
                << "HEIGHT 2\n"
                << "COLOR_SPACE \"sRGB\"\n"
                << "SOURCE \"decoded.bmp\"\n"
                << "END_TEXTURE\n";
        }

        assets::CachedArtifactSet
            decoded_cached;

        decoded_cached.fingerprint =
            "bmp-v1";

        decoded_cached.importer_id =
            "NEngine.Texture";

        decoded_cached.artifacts.push_back({
            bmp_source,
            "source"
        });

        decoded_cached.artifacts.push_back({
            bmp_descriptor,
            "texture-descriptor"
        });

        const auto decoded_guid =
            assets::AssetGuid::generate();

        render::DecodedTextureCache
            decoded_cache;

        const auto* decoded =
            decoded_cache.load(
                decoded_guid,
                decoded_cached,
                &resolve_error);

        check(
            decoded &&
            decoded->valid() &&
            decoded->width == 2u &&
            decoded->height == 2u &&
            decoded->rgba8.size() == 16u &&
            decoded->rgba8[0] == 255u &&
            decoded->rgba8[1] == 0u &&
            decoded->rgba8[2] == 0u &&
            decoded->rgba8[4] == 0u &&
            decoded->rgba8[5] == 255u &&
            decoded->rgba8[8] == 0u &&
            decoded->rgba8[9] == 0u &&
            decoded->rgba8[10] == 255u &&
            decoded_cache.size() == 1u &&
            decoded_cache.find(decoded_guid) ==
                decoded,
            "decoded texture cache resolves BMP AssetGuid into top-left RGBA8 pixels");

        const auto* cached_again =
            decoded_cache.load(
                decoded_guid,
                decoded_cached,
                &resolve_error);

        check(
            cached_again == decoded &&
            decoded_cache.size() == 1u,
            "decoded texture cache reuses matching AssetGuid fingerprint");

        // Change top-left pixel from red to yellow and advance
        // the import fingerprint. The cache must decode again.
        bmp_bytes[62] = 0u;
        bmp_bytes[63] = 255u;
        bmp_bytes[64] = 255u;

        {
            std::ofstream output(
                bmp_source,
                std::ios::binary |
                    std::ios::trunc);

            output.write(
                reinterpret_cast<const char*>(
                    bmp_bytes.data()),
                static_cast<std::streamsize>(
                    bmp_bytes.size()));
        }

        decoded_cached.fingerprint =
            "bmp-v2";

        const auto* refreshed =
            decoded_cache.load(
                decoded_guid,
                decoded_cached,
                &resolve_error);

        check(
            refreshed &&
            refreshed->rgba8[0] == 255u &&
            refreshed->rgba8[1] == 255u &&
            refreshed->rgba8[2] == 0u &&
            decoded_cache.size() == 1u,
            "decoded texture cache invalidates stale pixels when import fingerprint changes");

        const auto tga_source =
            root / "decoded.tga";

        std::array<std::uint8_t, 24>
            tga_bytes{};

        tga_bytes[2] = 2u;
        tga_bytes[12] = 2u;
        tga_bytes[14] = 1u;
        tga_bytes[16] = 24u;
        tga_bytes[17] = 0x20u;

        // Top-left origin: red then blue.
        tga_bytes[18] = 0u;
        tga_bytes[19] = 0u;
        tga_bytes[20] = 255u;
        tga_bytes[21] = 255u;
        tga_bytes[22] = 0u;
        tga_bytes[23] = 0u;

        {
            std::ofstream output(
                tga_source,
                std::ios::binary |
                    std::ios::trunc);

            output.write(
                reinterpret_cast<const char*>(
                    tga_bytes.data()),
                static_cast<std::streamsize>(
                    tga_bytes.size()));
        }

        render::ResolvedTextureAsset
            resolved_tga;

        resolved_tga.guid =
            assets::AssetGuid::generate();
        resolved_tga.metadata.format =
            "tga";
        resolved_tga.metadata.width = 2u;
        resolved_tga.metadata.height = 1u;
        resolved_tga.metadata.color_space =
            "sRGB";
        resolved_tga.source_path =
            tga_source;

        render::DecodedTextureData
            decoded_tga;

        check(
            render::decode_texture_rgba8(
                resolved_tga,
                decoded_tga,
                &resolve_error) &&
            decoded_tga.valid() &&
            decoded_tga.rgba8[0] == 255u &&
            decoded_tga.rgba8[1] == 0u &&
            decoded_tga.rgba8[2] == 0u &&
            decoded_tga.rgba8[4] == 0u &&
            decoded_tga.rgba8[5] == 0u &&
            decoded_tga.rgba8[6] == 255u,
            "TGA decoder produces normalized top-left RGBA8 pixels");

        const auto gltf_source =
            root / "triangle.gltf";

        {
            std::ofstream output(
                gltf_source,
                std::ios::binary |
                    std::ios::trunc);

            output
                << R"json({
  "asset":{"version":"2.0"},
  "buffers":[{
    "byteLength":102,
    "uri":"data:application/octet-stream;base64,AAAAvwAAAL8AAAAAAAAAPwAAAL8AAAAAAAAAAAAAAD8AAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAD8AAIA/AAABAAIA"
  }],
  "bufferViews":[
    {"buffer":0,"byteOffset":0,"byteLength":36},
    {"buffer":0,"byteOffset":36,"byteLength":36},
    {"buffer":0,"byteOffset":72,"byteLength":24},
    {"buffer":0,"byteOffset":96,"byteLength":6}
  ],
  "accessors":[
    {"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},
    {"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"},
    {"bufferView":2,"componentType":5126,"count":3,"type":"VEC2"},
    {"bufferView":3,"componentType":5123,"count":3,"type":"SCALAR"}
  ],
  "meshes":[{
    "primitives":[{
      "attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},
      "indices":3
    }]
  }]
})json";
        }

        render::ResolvedModelAsset
            gltf_asset;

        gltf_asset.guid =
            assets::AssetGuid::generate();
        gltf_asset.metadata.format =
            ".gltf";
        gltf_asset.source_path =
            gltf_source;

        check(
            render::decode_gltf_mesh(
                gltf_asset,
                decoded_gltf_fixture,
                &resolve_error) &&
            decoded_gltf_fixture.valid() &&
            decoded_gltf_fixture.vertices.size() == 3u &&
            decoded_gltf_fixture.indices ==
                std::vector<std::uint32_t>{
                    0u, 2u, 1u} &&
            decoded_gltf_fixture.vertices[0]
                .position.x == -0.5f &&
            decoded_gltf_fixture.vertices[1]
                .uv.x == 1.0f &&
            decoded_gltf_fixture.vertices[2]
                .normal.z == -1.0f &&
            decoded_gltf_fixture.bounds.center.x == 0.0f &&
            decoded_gltf_fixture.bounds.center.y == 0.0f &&
            decoded_gltf_fixture.bounds.extents.x == 0.5f &&
            decoded_gltf_fixture.bounds.extents.y == 0.5f,
            "glTF 2.0 embedded-buffer triangle decodes into MeshData");

        const auto model_descriptor =
            root / "model.nasset";

        {
            std::ofstream output(
                model_descriptor,
                std::ios::binary |
                    std::ios::trunc);

            output
                << "NENGINE_MODEL 1\n"
                << "FORMAT \".gltf\"\n"
                << "SOURCE \"triangle.gltf\"\n"
                << "SOURCE_BYTES "
                << std::filesystem::file_size(
                    gltf_source)
                << "\n"
                << "END_MODEL\n";
        }

        assets::CachedArtifactSet
            model_cached;

        model_cached.fingerprint =
            "gltf-v1";
        model_cached.importer_id =
            "NEngine.Model";

        model_cached.artifacts.push_back({
            gltf_source,
            "source"
        });

        model_cached.artifacts.push_back({
            model_descriptor,
            "model-descriptor"
        });

        render::DecodedMeshCache
            decoded_mesh_cache;

        const auto* cached_mesh =
            decoded_mesh_cache.load(
                gltf_asset.guid,
                model_cached,
                &resolve_error);

        check(
            cached_mesh &&
            cached_mesh->valid() &&
            cached_mesh->vertices.size() == 3u &&
            cached_mesh->indices.size() == 3u &&
            decoded_mesh_cache.size() == 1u &&
            decoded_mesh_cache.find(
                gltf_asset.guid) ==
                cached_mesh,
            "AssetGuid decoded mesh cache resolves imported glTF artifacts");

        const auto* cached_mesh_again =
            decoded_mesh_cache.load(
                gltf_asset.guid,
                model_cached,
                &resolve_error);

        check(
            cached_mesh_again == cached_mesh &&
            decoded_mesh_cache.size() == 1u,
            "decoded mesh cache reuses matching import fingerprint");

        const auto shader_source =
            root / "source.vert.spv";

        const auto shader_descriptor =
            root / "shader.nasset";

        const std::array<
            std::uint32_t,
            5> shader_words{
                0x07230203u,
                0x00010000u,
                0u,
                1u,
                0u
            };

        {
            std::ofstream output(
                shader_source,
                std::ios::binary |
                    std::ios::trunc);

            output.write(
                reinterpret_cast<
                    const char*>(
                        shader_words.data()),
                static_cast<
                    std::streamsize>(
                        sizeof(shader_words)));
        }

        {
            std::ofstream output(
                shader_descriptor,
                std::ios::binary |
                    std::ios::trunc);

            output
                << "NENGINE_SHADER 1\n"
                << "FORMAT \"spirv\"\n"
                << "STAGE \"vertex\"\n"
                << "WORDS 5\n"
                << "SOURCE \"source.vert.spv\"\n"
                << "END_SHADER\n";
        }

        assets::CachedArtifactSet
            shader_cached;

        shader_cached.importer_id =
            "NEngine.Shader";

        shader_cached.artifacts.push_back({
            shader_source,
            "source"
        });

        shader_cached.artifacts.push_back({
            shader_descriptor,
            "shader-descriptor"
        });

        const auto shader_guid =
            assets::AssetGuid::generate();

        const auto resolved_shader =
            render::resolve_shader_asset(
                shader_guid,
                shader_cached,
                &resolve_error);

        check(
            resolved_shader.has_value() &&
            resolved_shader->guid ==
                shader_guid &&
            resolved_shader->metadata.stage ==
                "vertex" &&
            resolved_shader->spirv ==
                std::vector<std::uint32_t>(
                    shader_words.begin(),
                    shader_words.end()),
            "renderer resolves shader GUID into validated SPIR-V words");

        {
            std::ofstream output(
                shader_descriptor,
                std::ios::binary |
                    std::ios::trunc);

            output
                << "NENGINE_SHADER 1\n"
                << "FORMAT \"spirv\"\n"
                << "STAGE \"vertex\"\n"
                << "WORDS 6\n"
                << "SOURCE \"source.vert.spv\"\n"
                << "END_SHADER\n";
        }

        check(
            !render::resolve_shader_asset(
                shader_guid,
                shader_cached,
                &resolve_error).has_value(),
            "renderer rejects shader descriptor/binary word-count mismatch");

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

                    {
                        const std::uint32_t sample_data[] = {
                            11u,
                            22u,
                            33u,
                            44u
                        };

                        render::VulkanBufferResource
                            host_buffer;

                        check(
                            host_buffer.create(
                                loader,
                                instance,
                                device,
                                sizeof(sample_data),
                                render::VulkanBufferUsage::Uniform,
                                render::VulkanMemoryPreference::HostVisible,
                                sample_data) &&
                            host_buffer.valid() &&
                            host_buffer.host_visible(),
                            "Vulkan host-visible buffer creates and accepts initial upload");

                        render::VulkanBufferResource
                            device_buffer;

                        check(
                            device_buffer.create(
                                loader,
                                instance,
                                device,
                                sizeof(sample_data),
                                render::VulkanBufferUsage::Vertex,
                                render::VulkanMemoryPreference::DeviceLocal,
                                sample_data) &&
                            device_buffer.valid() &&
                            !device_buffer.host_visible(),
                            "Vulkan device-local buffer stages initial data");

                        const std::uint8_t texture_pixels[] = {
                            255u,   0u,   0u, 255u,
                              0u, 255u,   0u, 255u,
                              0u,   0u, 255u, 255u,
                            255u, 255u, 255u, 255u
                        };

                        render::VulkanTextureResource
                            gpu_texture;

                        check(
                            gpu_texture.create_rgba8(
                                loader,
                                instance,
                                device,
                                2u,
                                2u,
                                texture_pixels,
                                sizeof(texture_pixels),
                                render::VulkanTextureColorSpace::Linear) &&
                            gpu_texture.valid() &&
                            gpu_texture.width() == 2u &&
                            gpu_texture.height() == 2u &&
                            gpu_texture.native_view() != nullptr &&
                            gpu_texture.native_sampler() != nullptr,
                            "Vulkan RGBA8 texture stages pixels and creates sampled image/view/sampler");

                        render::VulkanTextureAssetCache
                            gpu_texture_cache;

                        check(
                            gpu_texture_cache.initialize(
                                loader,
                                instance,
                                device),
                            "Vulkan texture asset cache initializes for headless device");

                        render::DecodedTextureData
                            decoded_asset_texture;

                        decoded_asset_texture.width = 2u;
                        decoded_asset_texture.height = 2u;
                        decoded_asset_texture.color_space =
                            render::DecodedTextureColorSpace::Linear;
                        decoded_asset_texture.rgba8.assign(
                            std::begin(texture_pixels),
                            std::end(texture_pixels));

                        const auto gpu_asset_guid =
                            assets::AssetGuid::generate();

                        std::string gpu_asset_error;

                        const auto* gpu_asset =
                            gpu_texture_cache.upload(
                                gpu_asset_guid,
                                "gpu-v1",
                                decoded_asset_texture,
                                &gpu_asset_error);

                        check(
                            gpu_asset &&
                            gpu_asset->valid() &&
                            gpu_asset->texture.width() == 2u &&
                            gpu_asset->texture.height() == 2u &&
                            gpu_texture_cache.find(
                                gpu_asset_guid) ==
                                gpu_asset &&
                            gpu_texture_cache.size() == 1u,
                            "decoded AssetGuid texture uploads into per-device Vulkan texture/material cache");

                        const auto* gpu_asset_again =
                            gpu_texture_cache.upload(
                                gpu_asset_guid,
                                "gpu-v1",
                                decoded_asset_texture,
                                &gpu_asset_error);

                        check(
                            gpu_asset_again == gpu_asset &&
                            gpu_texture_cache.size() == 1u,
                            "Vulkan texture asset cache reuses matching fingerprint");

                        render::VulkanMaterialResource
                            gpu_material;

                        check(
                            gpu_material.create_textured(
                                device,
                                gpu_texture) &&
                            gpu_material.valid() &&
                            gpu_material.native_descriptor_set_layout() != nullptr &&
                            gpu_material.native_descriptor_set() != nullptr,
                            "Vulkan textured material creates descriptor layout pool and combined image sampler set");

                        render::VulkanMeshResource
                            gpu_cube;

                        const auto cpu_cube =
                            render::make_unit_cube_mesh();

                        check(
                            gpu_cube.create(
                                loader,
                                instance,
                                device,
                                cpu_cube) &&
                            gpu_cube.valid() &&
                            gpu_cube.index_count() == 36,
                            "Vulkan mesh resource uploads built-in cube vertex and index buffers");

                        render::VulkanMeshResource
                            gpu_gltf;

                        check(
                            decoded_gltf_fixture.valid() &&
                            gpu_gltf.create(
                                loader,
                                instance,
                                device,
                                decoded_gltf_fixture) &&
                            gpu_gltf.valid() &&
                            gpu_gltf.index_count() == 3u,
                            "decoded glTF MeshData uploads through existing Vulkan mesh path");

                        render::VulkanMeshAssetCache
                            gpu_mesh_asset_cache;

                        check(
                            gpu_mesh_asset_cache.initialize(
                                loader,
                                instance,
                                device),
                            "Vulkan mesh AssetGuid cache initializes for headless device");

                        const auto gpu_mesh_guid =
                            assets::AssetGuid::generate();

                        std::string gpu_mesh_error;

                        const auto* cached_gpu_mesh =
                            gpu_mesh_asset_cache.upload(
                                gpu_mesh_guid,
                                "mesh-v1",
                                decoded_gltf_fixture,
                                &gpu_mesh_error);

                        check(
                            cached_gpu_mesh &&
                            cached_gpu_mesh->valid() &&
                            cached_gpu_mesh->index_count() == 3u &&
                            gpu_mesh_asset_cache.find(
                                gpu_mesh_guid) ==
                                cached_gpu_mesh &&
                            gpu_mesh_asset_cache.size() == 1u,
                            "decoded AssetGuid mesh uploads into per-device Vulkan mesh cache");

                        const auto* cached_gpu_mesh_again =
                            gpu_mesh_asset_cache.upload(
                                gpu_mesh_guid,
                                "mesh-v1",
                                decoded_gltf_fixture,
                                &gpu_mesh_error);

                        check(
                            cached_gpu_mesh_again ==
                                cached_gpu_mesh &&
                            gpu_mesh_asset_cache.size() == 1u,
                            "Vulkan mesh AssetGuid cache reuses matching fingerprint");

                        render::VulkanShaderModule
                            diagnostic_vertex;

                        render::VulkanShaderModule
                            diagnostic_fragment;

                        render::VulkanShaderModule
                            diagnostic_textured_vertex;

                        render::VulkanShaderModule
                            diagnostic_textured_fragment;

                        check(
                            diagnostic_vertex.create(
                                device,
                                render::VulkanShaderStage::Vertex,
                                render::diagnostic_vertex_spirv()),
                            "built-in diagnostic vertex SPIR-V creates Vulkan shader module");

                        check(
                            diagnostic_fragment.create(
                                device,
                                render::VulkanShaderStage::Fragment,
                                render::diagnostic_fragment_spirv()),
                            "built-in diagnostic fragment SPIR-V creates Vulkan shader module");

                        check(
                            diagnostic_textured_vertex.create(
                                device,
                                render::VulkanShaderStage::Vertex,
                                render::diagnostic_textured_vertex_spirv()),
                            "diagnostic textured vertex SPIR-V creates Vulkan shader module");

                        check(
                            diagnostic_textured_fragment.create(
                                device,
                                render::VulkanShaderStage::Fragment,
                                render::diagnostic_textured_fragment_spirv()),
                            "diagnostic textured fragment SPIR-V creates Vulkan shader module");

                        render::VulkanRenderPass
                            headless_render_pass;

                        // VK_FORMAT_R8G8B8A8_UNORM. A standalone
                        // render pass does not require a window or
                        // framebuffer and lets CI validate pipeline
                        // creation on a headless Vulkan device.
                        check(
                            headless_render_pass.create_color(
                                device,
                                37u),
                            "headless Vulkan color render pass creates");

                        render::VulkanGraphicsPipeline
                            headless_pipeline;

                        check(
                            headless_render_pass.valid() &&
                            diagnostic_vertex.valid() &&
                            diagnostic_fragment.valid() &&
                            headless_pipeline.create(
                                device,
                                headless_render_pass,
                                diagnostic_vertex,
                                diagnostic_fragment) &&
                            !headless_pipeline
                                .supports_material_descriptors(),
                            "diagnostic SPIR-V creates real Vulkan graphics pipeline without material descriptors");

                        render::VulkanGraphicsPipeline
                            textured_pipeline;

                        check(
                            headless_render_pass.valid() &&
                            gpu_material.valid() &&
                            diagnostic_textured_vertex.valid() &&
                            diagnostic_textured_fragment.valid() &&
                            textured_pipeline.create(
                                device,
                                headless_render_pass,
                                diagnostic_textured_vertex,
                                diagnostic_textured_fragment,
                                gpu_material) &&
                            textured_pipeline
                                .supports_material_descriptors(),
                            "texture-sampling SPIR-V creates a material-compatible Vulkan graphics pipeline");

                        render::VulkanDepthTarget
                            headless_depth;

                        check(
                            headless_depth.create(
                                loader,
                                instance,
                                device,
                                64u,
                                64u) &&
                            headless_depth.valid(),
                            "headless Vulkan depth image allocates and creates view");

                        render::VulkanRenderPass
                            depth_render_pass;

                        check(
                            headless_depth.valid() &&
                            depth_render_pass.create_color_depth(
                                device,
                                37u,
                                headless_depth.format()),
                            "headless Vulkan color+depth render pass creates");

                        render::VulkanGraphicsPipeline
                            depth_pipeline;

                        check(
                            depth_render_pass.valid() &&
                            depth_render_pass.has_depth() &&
                            depth_pipeline.create(
                                device,
                                depth_render_pass,
                                diagnostic_vertex,
                                diagnostic_fragment),
                            "diagnostic Vulkan pipeline creates with depth test state");

                        depth_pipeline.destroy();
                        depth_render_pass.destroy();
                        headless_depth.destroy();
                        textured_pipeline.destroy();
                        headless_pipeline.destroy();
                        headless_render_pass.destroy();
                        diagnostic_textured_fragment.destroy();
                        diagnostic_textured_vertex.destroy();
                        diagnostic_fragment.destroy();
                        diagnostic_vertex.destroy();
                        gpu_mesh_asset_cache.shutdown();
                        gpu_gltf.destroy();
                        gpu_cube.destroy();
                        gpu_material.destroy();
                        gpu_texture_cache.shutdown();
                        gpu_texture.destroy();
                        device_buffer.destroy();
                        host_buffer.destroy();
                    }

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
        core::World matrix_world;

        const auto parent =
            matrix_world.create("Parent");

        const auto child =
            matrix_world.create("Child");

        matrix_world.transform(parent)
            ->local_position =
            {10.0f, 0.0f, 0.0f};

        matrix_world.transform(child)
            ->local_position =
            {2.0f, 3.0f, 4.0f};

        matrix_world.set_parent(
            child,
            parent);

        const auto child_world =
            render::world_matrix(
                matrix_world,
                child);

        const auto world_origin =
            render::transform_point(
                child_world,
                {0.0f, 0.0f, 0.0f});

        check(
            world_origin ==
                core::Vec3{
                    12.0f,
                    3.0f,
                    4.0f},
            "renderer world matrix composes parent and child transforms");

        const auto inverse =
            render::inverse_affine(
                child_world);

        check(
            inverse.has_value(),
            "renderer affine world matrix is invertible");

        if (inverse) {
            const auto local_origin =
                render::transform_point(
                    *inverse,
                    world_origin);

            check(
                std::fabs(
                    local_origin.x) <
                        1.0e-5f &&
                std::fabs(
                    local_origin.y) <
                        1.0e-5f &&
                std::fabs(
                    local_origin.z) <
                        1.0e-5f,
                "renderer affine inverse returns world point to local origin");
        }

        const auto camera_entity =
            matrix_world.create(
                "Matrix Camera");

        matrix_world.transform(
            camera_entity)
            ->local_position =
            {0.0f, 0.0f, -8.0f};

        auto* matrix_camera =
            matrix_world.add_component<
                render::Camera>(
                    camera_entity,
                    render::camera_type());

        check(
            matrix_camera != nullptr,
            "matrix test Camera attaches");

        const auto matrices =
            render::build_camera_matrices(
                matrix_world,
                camera_entity,
                16.0f / 9.0f);

        check(
            matrices.has_value(),
            "Camera view/projection matrices build");

        if (matrices) {
            const auto view_origin =
                render::transform_point(
                    matrices->view,
                    {0.0f, 0.0f, 0.0f});

            check(
                std::fabs(
                    view_origin.z -
                    8.0f) <
                    1.0e-5f,
                "identity Camera at negative Z sees origin at positive view Z");
        }

        const auto projection =
            render::perspective_lh_zo(
                60.0f,
                1.0f,
                0.1f,
                100.0f);

        const auto near_point =
            render::transform_point(
                projection,
                {0.0f, 0.0f, 0.1f});

        const auto far_point =
            render::transform_point(
                projection,
                {0.0f, 0.0f, 100.0f});

        check(
            std::fabs(
                near_point.z) <
                1.0e-4f &&
            std::fabs(
                far_point.z -
                1.0f) <
                1.0e-4f,
            "Vulkan LH projection maps near/far depth to 0..1");
    }

    {
        const auto cube_guid =
            render::builtin_unit_cube_mesh_guid();

        const auto quad_guid =
            render::builtin_unit_quad_mesh_guid();

        check(
            cube_guid.valid() &&
            quad_guid.valid() &&
            cube_guid != quad_guid,
            "built-in mesh AssetGuids are stable valid and distinct");

        const auto builtin_cube =
            render::builtin_mesh_data(
                cube_guid);

        const auto builtin_quad =
            render::builtin_mesh_data(
                quad_guid);

        check(
            builtin_cube.has_value() &&
            builtin_cube->vertices.size() == 24 &&
            builtin_cube->indices.size() == 36,
            "built-in cube AssetGuid resolves to expected CPU mesh");

        check(
            builtin_quad.has_value() &&
            builtin_quad->vertices.size() == 4 &&
            builtin_quad->indices.size() == 6,
            "built-in quad AssetGuid resolves to expected CPU mesh");

        check(
            !render::builtin_mesh_data(
                assets::AssetGuid::generate())
                .has_value(),
            "unknown AssetGuid does not resolve as built-in mesh");
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

    {
        render::VulkanMaterialResource material;

        check(
            !material.valid(),
            "default Vulkan material resource is invalid");

        render::VulkanMeshDraw draw;

        check(
            draw.material == nullptr,
            "default Vulkan mesh draw has no material descriptor binding");
    }

    {
        render::VulkanTextureResource texture;

        check(
            !texture.valid(),
            "default Vulkan texture resource is invalid");
    }

    {
        render::VulkanDepthTarget depth;

        check(
            !depth.valid(),
            "default Vulkan depth target is invalid");
    }

    {
        render::VulkanGraphicsPipeline pipeline;
        render::VulkanDevice no_device;
        render::VulkanRenderTargets no_targets;
        render::VulkanShaderModule no_vertex;
        render::VulkanShaderModule no_fragment;

        check(
            !pipeline.valid(),
            "default Vulkan graphics pipeline is invalid");

        check(
            !pipeline.create(
                no_device,
                no_targets,
                no_vertex,
                no_fragment) &&
            !pipeline.diagnostic().empty(),
            "Vulkan graphics pipeline rejects missing runtime resources");
    }

    {
        render::VulkanShaderModule shader;
        render::VulkanDevice no_device;

        check(
            !shader.valid(),
            "default Vulkan shader module is invalid");

        check(
            !shader.create(
                no_device,
                render::VulkanShaderStage::Vertex,
                {
                    0xDEADBEEFu,
                    0u,
                    0u,
                    0u,
                    0u
                }) &&
            shader.diagnostic().find(
                "SPIR-V") !=
                std::string::npos,
            "Vulkan shader module rejects invalid SPIR-V before device access");
    }

    {
        render::VulkanMeshResource mesh;

        check(
            !mesh.valid() &&
            mesh.index_count() == 0,
            "default Vulkan mesh resource is invalid");
    }

    {
        render::VulkanBufferResource buffer;

        check(
            !buffer.valid(),
            "default Vulkan buffer resource is invalid");

        check(
            !buffer.upload(
                "x",
                1,
                0) &&
            !buffer.diagnostic().empty(),
            "uninitialized Vulkan buffer upload fails diagnostically");
    }

    {
        render::VulkanClearPresenter presenter;

        check(
            !presenter.ready(),
            "default Vulkan presenter is not ready");

        check(
            !presenter.present_clear(
                0.1f,
                0.2f,
                0.3f,
                1.0f) &&
            !presenter.diagnostic().empty(),
            "uninitialized Vulkan presenter fails diagnostically");
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
