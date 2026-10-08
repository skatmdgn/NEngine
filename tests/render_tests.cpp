#include <algorithm>
#include <array>
#include <cstdint>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "nengine/assets/asset_guid.hpp"
#include "nengine/assets/builtin_processors.hpp"
#include "nengine/assets/import_pipeline.hpp"
#include "nengine/core/scene.hpp"
#include "nengine/core/world.hpp"
#include "nengine/render/asset_resources.hpp"
#include "nengine/render/builtin_assets.hpp"
#include "nengine/render/components.hpp"
#include "nengine/render/decoded_mesh.hpp"
#include "nengine/render/decoded_texture.hpp"
#include "nengine/render/diagnostic_shaders.hpp"
#include "nengine/render/gltf_mesh.hpp"
#include "nengine/render/material_asset.hpp"
#include "nengine/render/matrix.hpp"
#include "nengine/render/mesh_data.hpp"
#include "nengine/render/obj_mesh.hpp"
#include "nengine/render/registration.hpp"
#include "nengine/render/render_snapshot.hpp"
#include "nengine/render/rhi.hpp"
#include "nengine/render/vulkan_buffer.hpp"
#include "nengine/render/vulkan_loader.hpp"
#include "nengine/render/vulkan_material.hpp"
#include "nengine/render/vulkan_material_asset_cache.hpp"
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
    render::MeshData decoded_multi_material_fixture;

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

        const auto png_source =
            root / "decoded.png";

        const std::array<std::uint8_t, 74>
            png_bytes{
                0x89u, 0x50u, 0x4Eu, 0x47u, 0x0Du, 0x0Au, 0x1Au, 0x0Au,
                0x00u, 0x00u, 0x00u, 0x0Du, 0x49u, 0x48u, 0x44u, 0x52u,
                0x00u, 0x00u, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u, 0x01u,
                0x08u, 0x06u, 0x00u, 0x00u, 0x00u, 0xF4u, 0x22u, 0x7Fu,
                0x8Au, 0x00u, 0x00u, 0x00u, 0x11u, 0x49u, 0x44u, 0x41u,
                0x54u, 0x78u, 0xDAu, 0x63u, 0xF8u, 0xCFu, 0xC0u, 0xF0u,
                0x9Fu, 0xE1u, 0x3Fu, 0xC3u, 0x7Fu, 0x00u, 0x10u, 0xF8u,
                0x03u, 0xFDu, 0x3Cu, 0x9Fu, 0xE6u, 0xF6u, 0x00u, 0x00u,
                0x00u, 0x00u, 0x49u, 0x45u, 0x4Eu, 0x44u, 0xAEu, 0x42u,
                0x60u, 0x82u
            };

        {
            std::ofstream output(
                png_source,
                std::ios::binary |
                    std::ios::trunc);

            output.write(
                reinterpret_cast<const char*>(
                    png_bytes.data()),
                static_cast<std::streamsize>(
                    png_bytes.size()));
        }

        render::ResolvedTextureAsset
            resolved_png;

        resolved_png.guid =
            assets::AssetGuid::generate();
        resolved_png.metadata.format =
            "png";
        resolved_png.metadata.width = 2u;
        resolved_png.metadata.height = 1u;
        resolved_png.metadata.color_space =
            "sRGB";
        resolved_png.source_path =
            png_source;

        render::DecodedTextureData
            decoded_png;

        check(
            render::decode_texture_rgba8(
                resolved_png,
                decoded_png,
                &resolve_error) &&
            decoded_png.valid() &&
            decoded_png.width == 2u &&
            decoded_png.height == 1u &&
            decoded_png.rgba8[0] == 255u &&
            decoded_png.rgba8[1] == 0u &&
            decoded_png.rgba8[2] == 0u &&
            decoded_png.rgba8[3] == 255u &&
            decoded_png.rgba8[4] == 0u &&
            decoded_png.rgba8[5] == 255u &&
            decoded_png.rgba8[6] == 0u &&
            decoded_png.rgba8[7] == 255u,
            "pinned stb_image decodes PNG source into RGBA8 pixels");

        const auto material_source =
            root / "checker.nmat";

        const auto material_guid =
            assets::AssetGuid::generate();

        {
            std::ofstream output(
                material_source,
                std::ios::binary |
                    std::ios::trunc);

            output
                << "NENGINE_MATERIAL 1\n"
                << "BASE_COLOR_TEXTURE \""
                << resolved_png.guid.to_string()
                << "\"\n"
                << "END_MATERIAL\n";
        }

        assets::CachedArtifactSet
            material_cached;

        material_cached.fingerprint =
            "nmat-v1";
        material_cached.importer_id =
            "NEngine.Material";
        material_cached.artifacts.push_back({
            material_source,
            "source"
        });

        const auto resolved_material =
            render::resolve_material_asset(
                material_guid,
                material_cached,
                &resolve_error);

        check(
            resolved_material.has_value() &&
            resolved_material->guid ==
                material_guid &&
            resolved_material->material
                .base_color_texture ==
                resolved_png.guid,
            "renderer resolves nmat Material AssetGuid to base-color texture GUID");

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
      "indices":3,
      "material":0
    }]
  }],
  "materials":[{"pbrMetallicRoughness":{"baseColorTexture":{"index":0}}}],
  "textures":[{"source":0}],
  "images":[{"uri":"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAIAAAABCAYAAAD0In+KAAAAEUlEQVR42mP4z8Dwn+E/w38AEPgD/Tyf5vYAAAAASUVORK5CYII="}]
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

        const auto node_transform_source =
            root / "node_transform_triangle.gltf";

        {
            std::ofstream output(
                node_transform_source,
                std::ios::binary |
                    std::ios::trunc);

            output << R"json({
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
  "meshes":[{"primitives":[{
    "attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},
    "indices":3
  }]}],
  "nodes":[
    {"translation":[1,2,3],"children":[1]},
    {"mesh":0,"rotation":[0,0,0.7071067811865476,0.7071067811865476],"scale":[2,1,1]}
  ],
  "scenes":[{"nodes":[0]}],
  "scene":0
})json";
        }

        auto node_transform_asset =
            gltf_asset;

        node_transform_asset.source_path =
            node_transform_source;

        render::MeshData
            node_transform_mesh;

        check(
            render::decode_gltf_mesh(
                node_transform_asset,
                node_transform_mesh,
                &resolve_error) &&
            node_transform_mesh.valid() &&
            node_transform_mesh.vertices.size() == 3u &&
            std::abs(
                node_transform_mesh.vertices[0]
                    .position.x - 1.5f) < 0.0001f &&
            std::abs(
                node_transform_mesh.vertices[0]
                    .position.y - 1.0f) < 0.0001f &&
            std::abs(
                node_transform_mesh.vertices[0]
                    .position.z + 3.0f) < 0.0001f &&
            std::abs(
                node_transform_mesh.vertices[1]
                    .position.x - 1.5f) < 0.0001f &&
            std::abs(
                node_transform_mesh.vertices[1]
                    .position.y - 3.0f) < 0.0001f &&
            std::abs(
                node_transform_mesh.vertices[2]
                    .position.x - 0.5f) < 0.0001f &&
            std::abs(
                node_transform_mesh.vertices[2]
                    .position.y - 2.0f) < 0.0001f &&
            node_transform_mesh.indices ==
                std::vector<std::uint32_t>{
                    0u, 2u, 1u} &&
            std::abs(
                node_transform_mesh.bounds.center.x -
                    1.0f) < 0.0001f &&
            std::abs(
                node_transform_mesh.bounds.center.y -
                    2.0f) < 0.0001f &&
            std::abs(
                node_transform_mesh.bounds.center.z +
                    3.0f) < 0.0001f &&
            std::abs(
                node_transform_mesh.bounds.extents.x -
                    0.5f) < 0.0001f &&
            std::abs(
                node_transform_mesh.bounds.extents.y -
                    1.0f) < 0.0001f &&
            std::abs(
                node_transform_mesh.vertices[0]
                    .normal.z + 1.0f) < 0.0001f,
            "glTF scene hierarchy bakes parent translation child rotation and nonuniform scale");

        const auto node_matrix_source =
            root / "node_matrix_triangle.gltf";

        {
            std::ofstream output(
                node_matrix_source,
                std::ios::binary |
                    std::ios::trunc);

            output << R"json({
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
  "meshes":[{"primitives":[{
    "attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},
    "indices":3
  }]}],
  "nodes":[{
    "mesh":0,
    "matrix":[1,0,0,0,0,1,0,0,0,0,1,0,4,5,6,1]
  }],
  "scenes":[{"nodes":[0]}]
})json";
        }

        node_transform_asset.source_path =
            node_matrix_source;

        render::MeshData
            node_matrix_mesh;

        check(
            render::decode_gltf_mesh(
                node_transform_asset,
                node_matrix_mesh,
                &resolve_error) &&
            node_matrix_mesh.valid() &&
            std::abs(
                node_matrix_mesh.vertices[0]
                    .position.x - 3.5f) < 0.0001f &&
            std::abs(
                node_matrix_mesh.vertices[0]
                    .position.y - 4.5f) < 0.0001f &&
            std::abs(
                node_matrix_mesh.vertices[0]
                    .position.z + 6.0f) < 0.0001f,
            "glTF explicit column-major node matrix bakes into MeshData");

        const auto mirrored_node_source =
            root / "mirrored_node_triangle.gltf";

        {
            std::ofstream output(
                mirrored_node_source,
                std::ios::binary |
                    std::ios::trunc);

            output << R"json({
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
  "meshes":[{"primitives":[{
    "attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},
    "indices":3
  }]}],
  "nodes":[{"mesh":0,"scale":[-1,1,1]}],
  "scenes":[{"nodes":[0]}]
})json";
        }

        node_transform_asset.source_path =
            mirrored_node_source;

        render::MeshData
            mirrored_node_mesh;

        check(
            render::decode_gltf_mesh(
                node_transform_asset,
                mirrored_node_mesh,
                &resolve_error) &&
            mirrored_node_mesh.valid() &&
            mirrored_node_mesh.indices ==
                std::vector<std::uint32_t>{
                    0u, 1u, 2u} &&
            std::abs(
                mirrored_node_mesh.vertices[0]
                    .position.x - 0.5f) < 0.0001f,
            "glTF negative node scale preserves front-face winding after handedness conversion");

        const auto cyclic_node_source =
            root / "cyclic_node_triangle.gltf";

        {
            std::ofstream output(
                cyclic_node_source,
                std::ios::binary |
                    std::ios::trunc);

            output << R"json({
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
  "meshes":[{"primitives":[{
    "attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},
    "indices":3
  }]}],
  "nodes":[
    {"children":[1]},
    {"mesh":0,"children":[0]}
  ],
  "scenes":[{"nodes":[0]}]
})json";
        }

        node_transform_asset.source_path =
            cyclic_node_source;

        render::MeshData
            cyclic_node_mesh;

        std::string
            cyclic_node_error;

        check(
            !render::decode_gltf_mesh(
                node_transform_asset,
                cyclic_node_mesh,
                &cyclic_node_error) &&
            cyclic_node_error.find(
                "cycle") !=
                std::string::npos,
            "glTF importer rejects cyclic node graphs");

        const auto multi_material_source =
            root / "multi_material_triangle.gltf";

        {
            std::ofstream output(
                multi_material_source,
                std::ios::binary |
                    std::ios::trunc);

            output << R"json({
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
  "meshes":[{"primitives":[
    {
      "attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},
      "indices":3,
      "material":0
    },
    {
      "attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},
      "indices":3,
      "material":1
    }
  ]}],
  "materials":[
    {"pbrMetallicRoughness":{"baseColorFactor":[1,0,0,1]}},
    {"pbrMetallicRoughness":{"baseColorFactor":[0,0.5,1,1]}}
  ],
  "nodes":[{"mesh":0}],
  "scenes":[{"nodes":[0]}],
  "scene":0
})json";
        }

        auto multi_material_asset =
            gltf_asset;

        multi_material_asset.source_path =
            multi_material_source;

        check(
            render::decode_gltf_mesh(
                multi_material_asset,
                decoded_multi_material_fixture,
                &resolve_error) &&
            decoded_multi_material_fixture.valid() &&
            decoded_multi_material_fixture.vertices.size() == 6u &&
            decoded_multi_material_fixture.indices.size() == 6u &&
            decoded_multi_material_fixture.submeshes.size() == 2u &&
            decoded_multi_material_fixture.submeshes[0].first_index == 0u &&
            decoded_multi_material_fixture.submeshes[0].index_count == 3u &&
            decoded_multi_material_fixture.submeshes[0].material_slot == 0u &&
            decoded_multi_material_fixture.submeshes[1].first_index == 3u &&
            decoded_multi_material_fixture.submeshes[1].index_count == 3u &&
            decoded_multi_material_fixture.submeshes[1].material_slot == 1u,
            "glTF multiple primitives preserve independent submesh ranges and material slots");

        render::DecodedTextureData
            multi_material_red;
        render::DecodedTextureData
            multi_material_blue;

        check(
            render::decode_gltf_material_base_color_texture(
                multi_material_asset,
                0u,
                multi_material_red,
                &resolve_error) &&
            multi_material_red.valid() &&
            multi_material_red.width == 1u &&
            multi_material_red.height == 1u &&
            multi_material_red.rgba8 ==
                std::vector<std::uint8_t>{
                    255u, 0u, 0u, 255u},
            "glTF material slot 0 decodes independent red base color");

        check(
            render::decode_gltf_material_base_color_texture(
                multi_material_asset,
                1u,
                multi_material_blue,
                &resolve_error) &&
            multi_material_blue.valid() &&
            multi_material_blue.rgba8 ==
                std::vector<std::uint8_t>{
                    0u, 188u, 255u, 255u},
            "glTF material slot 1 decodes independent blue base color");

        const auto sparse_source =
            root / "sparse_position_triangle.gltf";

        {
            std::ofstream output(
                sparse_source,
                std::ios::binary |
                    std::ios::trunc);

            output << R"json({
  "asset":{"version":"2.0"},
  "buffers":[{
    "byteLength":40,
    "uri":"data:application/octet-stream;base64,AAECAAAAAL8AAAC/AAAAAAAAAD8AAAC/AAAAAAAAAAAAAAA/AAAAAA=="
  }],
  "bufferViews":[
    {"buffer":0,"byteOffset":0,"byteLength":3},
    {"buffer":0,"byteOffset":4,"byteLength":36}
  ],
  "accessors":[{
    "componentType":5126,
    "count":3,
    "type":"VEC3",
    "sparse":{
      "count":3,
      "indices":{"bufferView":0,"componentType":5121},
      "values":{"bufferView":1}
    }
  }],
  "meshes":[{"primitives":[{
    "attributes":{"POSITION":0}
  }]}]
})json";
        }

        auto sparse_asset =
            gltf_asset;
        sparse_asset.source_path =
            sparse_source;

        render::MeshData sparse_mesh;
        std::string sparse_error;

        check(
            render::decode_gltf_mesh(
                sparse_asset,
                sparse_mesh,
                &sparse_error) &&
            sparse_mesh.valid() &&
            sparse_mesh.vertices.size() == 3u &&
            sparse_mesh.indices ==
                std::vector<std::uint32_t>{
                    0u, 2u, 1u} &&
            std::abs(
                sparse_mesh.vertices[0]
                    .position.x + 0.5f) < 0.0001f &&
            std::abs(
                sparse_mesh.vertices[0]
                    .position.y + 0.5f) < 0.0001f &&
            std::abs(
                sparse_mesh.vertices[1]
                    .position.x - 0.5f) < 0.0001f &&
            std::abs(
                sparse_mesh.vertices[2]
                    .position.y - 0.5f) < 0.0001f &&
            sparse_mesh.submeshes.size() == 1u &&
            sparse_mesh.submeshes[0].material_slot ==
                render::kMeshMaterialUnassigned,
            "glTF sparse POSITION accessor overlays zero base storage into native MeshData");

        const auto invalid_sparse_source =
            root / "invalid_sparse_position.gltf";

        {
            std::ofstream output(
                invalid_sparse_source,
                std::ios::binary |
                    std::ios::trunc);

            output << R"json({
  "asset":{"version":"2.0"},
  "buffers":[{
    "byteLength":40,
    "uri":"data:application/octet-stream;base64,AAEBAAAAAL8AAAC/AAAAAAAAAD8AAAC/AAAAAAAAAAAAAAA/AAAAAA=="
  }],
  "bufferViews":[
    {"buffer":0,"byteOffset":0,"byteLength":3},
    {"buffer":0,"byteOffset":4,"byteLength":36}
  ],
  "accessors":[{
    "componentType":5126,
    "count":3,
    "type":"VEC3",
    "sparse":{
      "count":3,
      "indices":{"bufferView":0,"componentType":5121},
      "values":{"bufferView":1}
    }
  }],
  "meshes":[{"primitives":[{
    "attributes":{"POSITION":0}
  }]}]
})json";
        }

        sparse_asset.source_path =
            invalid_sparse_source;
        sparse_error.clear();

        check(
            !render::decode_gltf_mesh(
                sparse_asset,
                sparse_mesh,
                &sparse_error) &&
            sparse_error.find(
                "strictly increasing") !=
                    std::string::npos,
            "glTF sparse accessor rejects duplicate or unordered sparse indices");

        const auto sparse_indices_source =
            root / "sparse_indices_triangle.gltf";

        {
            std::ofstream output(
                sparse_indices_source,
                std::ios::binary |
                    std::ios::trunc);

            output << R"json({
  "asset":{"version":"2.0"},
  "buffers":[{
    "byteLength":44,
    "uri":"data:application/octet-stream;base64,AAAAvwAAAL8AAAAAAAAAPwAAAL8AAAAAAAAAAAAAAD8AAAAAAQIAAAIAAQA="
  }],
  "bufferViews":[
    {"buffer":0,"byteOffset":0,"byteLength":36},
    {"buffer":0,"byteOffset":36,"byteLength":2},
    {"buffer":0,"byteOffset":40,"byteLength":4}
  ],
  "accessors":[
    {"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},
    {
      "componentType":5123,
      "count":3,
      "type":"SCALAR",
      "sparse":{
        "count":2,
        "indices":{"bufferView":1,"componentType":5121},
        "values":{"bufferView":2}
      }
    }
  ],
  "meshes":[{"primitives":[{
    "attributes":{"POSITION":0},
    "indices":1
  }]}]
})json";
        }

        sparse_asset.source_path =
            sparse_indices_source;

        render::MeshData
            sparse_index_mesh;
        sparse_error.clear();

        check(
            render::decode_gltf_mesh(
                sparse_asset,
                sparse_index_mesh,
                &sparse_error) &&
            sparse_index_mesh.valid() &&
            sparse_index_mesh.indices ==
                std::vector<std::uint32_t>{
                    0u, 1u, 2u},
            "glTF sparse SCALAR index accessor overlays a zero base index stream");


        const auto quantized_source =
            root / "quantized_triangle.gltf";

        {
            std::ofstream output(
                quantized_source,
                std::ios::binary |
                    std::ios::trunc);

            output << R"json({
  "asset":{"version":"2.0"},
  "extensionsRequired":["KHR_mesh_quantization"],
  "buffers":[{
    "byteLength":54,
    "uri":"data:application/octet-stream;base64,AYABgAAAAAD/fwGAAAAAAAAA/38AAAAAAAB/AAAAfwAAAH8AAAAAAP//AAAAgP//AAABAAIA"
  }],
  "bufferViews":[
    {"buffer":0,"byteOffset":0,"byteLength":24,"byteStride":8},
    {"buffer":0,"byteOffset":24,"byteLength":12,"byteStride":4},
    {"buffer":0,"byteOffset":36,"byteLength":12},
    {"buffer":0,"byteOffset":48,"byteLength":6}
  ],
  "accessors":[
    {"bufferView":0,"componentType":5122,"normalized":true,"count":3,"type":"VEC3"},
    {"bufferView":1,"componentType":5120,"normalized":true,"count":3,"type":"VEC3"},
    {"bufferView":2,"componentType":5123,"normalized":true,"count":3,"type":"VEC2"},
    {"bufferView":3,"componentType":5123,"count":3,"type":"SCALAR"}
  ],
  "meshes":[{"primitives":[{
    "attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},
    "indices":3
  }]}],
  "nodes":[{"mesh":0,"scale":[0.5,0.5,1]}],
  "scenes":[{"nodes":[0]}],
  "scene":0
})json";
        }

        auto quantized_asset =
            gltf_asset;
        quantized_asset.source_path =
            quantized_source;

        render::MeshData quantized_mesh;
        std::string quantized_error;

        check(
            render::decode_gltf_mesh(
                quantized_asset,
                quantized_mesh,
                &quantized_error) &&
            quantized_mesh.valid() &&
            quantized_mesh.vertices.size() == 3u &&
            std::abs(
                quantized_mesh.vertices[0]
                    .position.x + 0.5f) < 0.0001f &&
            std::abs(
                quantized_mesh.vertices[0]
                    .position.y + 0.5f) < 0.0001f &&
            std::abs(
                quantized_mesh.vertices[1]
                    .position.x - 0.5f) < 0.0001f &&
            std::abs(
                quantized_mesh.vertices[2]
                    .position.y - 0.5f) < 0.0001f &&
            std::abs(
                quantized_mesh.vertices[0]
                    .normal.z + 1.0f) < 0.0001f &&
            std::abs(
                quantized_mesh.vertices[1]
                    .uv.x - 1.0f) < 0.0001f &&
            std::abs(
                quantized_mesh.vertices[2]
                    .uv.x - 0.5000076f) < 0.0001f &&
            std::abs(
                quantized_mesh.vertices[2]
                    .uv.y - 1.0f) < 0.0001f,
            "KHR_mesh_quantization SHORT/BYTE/USHORT normalized attributes decode through node dequantization");





        render::DecodedTextureData gltf_embedded_base_color;

        check(
            render::decode_gltf_base_color_texture(
                gltf_asset,
                gltf_embedded_base_color,
                &resolve_error) &&
            gltf_embedded_base_color.valid() &&
            gltf_embedded_base_color.width == 2u &&
            gltf_embedded_base_color.height == 1u &&
            gltf_embedded_base_color.color_space ==
                render::DecodedTextureColorSpace::SRgb &&
            gltf_embedded_base_color.rgba8 == decoded_png.rgba8,
            "glTF first primitive PBR baseColorTexture data URI decodes into RGBA8");

        // glTF baseColorFactor is linear, even for a texture uploaded as
        // Vulkan SRGB. Color-only materials must become visible too.
        const auto factor_only_path =
            root / "factor_only.gltf";

        {
            std::ofstream output(
                factor_only_path, std::ios::binary | std::ios::trunc);
            output << R"json({"asset":{"version":"2.0"},"meshes":[{"primitives":[{"material":0}]}],"materials":[{"pbrMetallicRoughness":{"baseColorFactor":[0.5,0.0,0.0,0.25]}}]})json";
        }

        auto factor_asset = gltf_asset;
        factor_asset.source_path = factor_only_path;
        render::DecodedTextureData factor_pixels;

        check(
            render::decode_gltf_base_color_texture(
                factor_asset, factor_pixels, &resolve_error) &&
            factor_pixels.valid() &&
            factor_pixels.width == 1u &&
            factor_pixels.height == 1u &&
            factor_pixels.rgba8 ==
                std::vector<std::uint8_t>{188u, 0u, 0u, 64u},
            "glTF color-only baseColorFactor generates correctly encoded 1x1 sRGB texture");

        const auto tinted_source = root / "tinted_triangle.gltf";

        {
            std::ifstream input(gltf_source, std::ios::binary);
            std::string original(
                std::istreambuf_iterator<char>{input},
                std::istreambuf_iterator<char>{});

            const std::string before =
                "\"baseColorTexture\":{\"index\":0}";
            const std::string after =
                "\"baseColorFactor\":[0.5,1.0,1.0,0.5],"
                "\"baseColorTexture\":{\"index\":0}";
            const auto index = original.find(before);
            if (index != std::string::npos) {
                original.replace(index, before.size(), after);
            }

            std::ofstream output(
                tinted_source, std::ios::binary | std::ios::trunc);
            output << original;
        }

        factor_asset.source_path = tinted_source;
        render::DecodedTextureData tinted_pixels;

        check(
            render::decode_gltf_base_color_texture(
                factor_asset, tinted_pixels, &resolve_error) &&
            tinted_pixels.valid() &&
            tinted_pixels.width == 2u &&
            tinted_pixels.height == 1u &&
            tinted_pixels.rgba8 ==
                std::vector<std::uint8_t>{
                    188u, 0u, 0u, 128u,
                    0u, 255u, 0u, 128u},
            "glTF texture image pixels tint by baseColorFactor in linear color space");

        {
            std::ofstream output(
                factor_only_path, std::ios::binary | std::ios::trunc);
            output << R"json({"asset":{"version":"2.0"},"meshes":[{"primitives":[{"material":0}]}],"materials":[{"pbrMetallicRoughness":{"baseColorFactor":[2,0,0,1]}}]})json";
        }

        factor_asset.source_path = factor_only_path;
        std::string invalid_factor_error;
        check(
            !render::decode_gltf_base_color_texture(
                factor_asset, factor_pixels, &invalid_factor_error) &&
            invalid_factor_error.find("baseColorFactor") !=
                std::string::npos,
            "glTF rejects out-of-range baseColorFactor input");


        // Real GLB 2.0 JSON + BIN chunks, with mesh accessors and an
        // image/png bufferView in the same BIN payload.
        const std::array<std::uint8_t, 102> glb_geometry{
                0x00u, 0x00u, 0x00u, 0xbfu, 0x00u, 0x00u, 0x00u, 0xbfu, 0x00u, 0x00u,
                0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x3fu, 0x00u, 0x00u, 0x00u, 0xbfu,
                0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
                0x00u, 0x3fu, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
                0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x80u, 0x3fu, 0x00u, 0x00u,
                0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x80u, 0x3fu,
                0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
                0x80u, 0x3fu, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
                0x00u, 0x00u, 0x80u, 0x3fu, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
                0x00u, 0x3fu, 0x00u, 0x00u, 0x80u, 0x3fu, 0x00u, 0x00u, 0x01u, 0x00u,
                0x02u, 0x00u
        };

        std::vector<std::uint8_t> glb_bin(180u, 0u);
        std::copy(
            glb_geometry.begin(),
            glb_geometry.end(),
            glb_bin.begin());
        std::copy(
            png_bytes.begin(),
            png_bytes.end(),
            glb_bin.begin() + 104);

        std::string glb_json = R"json({
  "asset":{"version":"2.0"},
  "buffers":[{"byteLength":178}],
  "bufferViews":[
    {"buffer":0,"byteOffset":0,"byteLength":36},
    {"buffer":0,"byteOffset":36,"byteLength":36},
    {"buffer":0,"byteOffset":72,"byteLength":24},
    {"buffer":0,"byteOffset":96,"byteLength":6},
    {"buffer":0,"byteOffset":104,"byteLength":74}
  ],
  "accessors":[
    {"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},
    {"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"},
    {"bufferView":2,"componentType":5126,"count":3,"type":"VEC2"},
    {"bufferView":3,"componentType":5123,"count":3,"type":"SCALAR"}
  ],
  "meshes":[{"primitives":[{
    "attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},
    "indices":3,
    "material":0
  }]}],
  "materials":[{"pbrMetallicRoughness":{"baseColorTexture":{"index":0}}}],
  "textures":[{"source":0}],
  "images":[{"bufferView":4,"mimeType":"image/png"}]
})json";

        while ((glb_json.size() % 4u) != 0u) {
            glb_json.push_back(' ');
        }

        const auto glb_source =
            root / "textured_triangle.glb";

        {
            std::ofstream output(
                glb_source,
                std::ios::binary |
                    std::ios::trunc);

            const auto write_u32 =
                [&](std::uint32_t value) {
                    const std::array<char, 4> encoded{
                        static_cast<char>(value & 0xFFu),
                        static_cast<char>((value >> 8u) & 0xFFu),
                        static_cast<char>((value >> 16u) & 0xFFu),
                        static_cast<char>((value >> 24u) & 0xFFu)
                    };
                    output.write(encoded.data(), 4);
                };

            write_u32(0x46546C67u);
            write_u32(2u);
            write_u32(static_cast<std::uint32_t>(
                12u + 8u + glb_json.size() + 8u + glb_bin.size()));
            write_u32(static_cast<std::uint32_t>(glb_json.size()));
            write_u32(0x4E4F534Au);
            output.write(
                glb_json.data(),
                static_cast<std::streamsize>(glb_json.size()));
            write_u32(static_cast<std::uint32_t>(glb_bin.size()));
            write_u32(0x004E4942u);
            output.write(
                reinterpret_cast<const char*>(glb_bin.data()),
                static_cast<std::streamsize>(glb_bin.size()));
        }

        auto glb_asset = gltf_asset;
        glb_asset.metadata.format = ".glb";
        glb_asset.source_path = glb_source;

        render::MeshData glb_decoded_mesh;
        render::DecodedTextureData glb_decoded_base_color;

        check(
            render::decode_gltf_mesh(
                glb_asset,
                glb_decoded_mesh,
                &resolve_error) &&
            glb_decoded_mesh.valid() &&
            glb_decoded_mesh.vertices.size() == 3u &&
            glb_decoded_mesh.indices ==
                std::vector<std::uint32_t>{0u, 2u, 1u},
            "GLB JSON and BIN chunks decode a real triangle mesh");

        check(
            render::decode_gltf_base_color_texture(
                glb_asset,
                glb_decoded_base_color,
                &resolve_error) &&
            glb_decoded_base_color.valid() &&
            glb_decoded_base_color.rgba8 == decoded_png.rgba8,
            "GLB PNG bufferView decodes first PBR base-color image");

        // A real multi-file .gltf must resolve the staged copies, not
        // the original sidecars in the model's source directory.
        const auto external_source_dir = root / "external_source";
        const auto external_gltf = external_source_dir / "triangle.gltf";
        const auto external_bin =
            external_source_dir / "geometry" / "triangle.bin";
        const auto external_png =
            external_source_dir / "images" / "albedo.png";

        std::filesystem::create_directories(
            external_bin.parent_path());
        std::filesystem::create_directories(
            external_png.parent_path());

        {
            std::ofstream output(
                external_bin, std::ios::binary | std::ios::trunc);
            output.write(
                reinterpret_cast<const char*>(glb_geometry.data()),
                static_cast<std::streamsize>(glb_geometry.size()));
        }
        {
            std::ofstream output(
                external_png, std::ios::binary | std::ios::trunc);
            output.write(
                reinterpret_cast<const char*>(png_bytes.data()),
                static_cast<std::streamsize>(png_bytes.size()));
        }
        {
            std::ofstream output(
                external_gltf, std::ios::binary | std::ios::trunc);
            output << R"json({
 "asset":{"version":"2.0"},
 "buffers":[{"uri":"geometry/triangle.bin","byteLength":102}],
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
 "meshes":[{"primitives":[{
    "attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},
    "indices":3,"material":0
 }]}],
 "materials":[{"pbrMetallicRoughness":{"baseColorTexture":{"index":0}}}],
 "textures":[{"source":0}],
 "images":[{"uri":"images/albedo.png"}]
})json";
        }

        assets::AssetRecord external_record;
        external_record.guid = assets::AssetGuid::generate();
        external_record.source_path = external_gltf;
        external_record.importer_id = "NEngine.Model";
        external_record.file_size =
            std::filesystem::file_size(external_gltf);
        external_record.write_stamp = 1;

        assets::ImporterRegistry external_registry;
        external_registry.register_importer({
            "NEngine.Model", 1, {".gltf"}, false
        });

        assets::AssetImportPipeline external_pipeline;
        external_pipeline.register_processor(
            "NEngine.Model", assets::model_source_importer);

        const auto external_cache_root = root / "external_cache";
        const auto external_import = external_pipeline.import(
            external_record, external_registry, external_cache_root);
        const auto external_artifacts =
            external_pipeline.cached_artifacts(
                external_record, external_registry, external_cache_root);

        check(
            external_import.success &&
            external_artifacts.has_value() &&
            external_artifacts->artifacts.size() == 4u,
            "multi-file glTF importer caches source descriptor bin and PNG sidecars");

        if (external_artifacts) {
            const auto resolved =
                render::resolve_model_asset(
                    external_record.guid, *external_artifacts, &resolve_error);

            render::MeshData external_mesh;
            render::DecodedTextureData external_base_color;

            check(
                resolved.has_value() &&
                render::decode_gltf_mesh(
                    *resolved, external_mesh, &resolve_error) &&
                external_mesh.valid() &&
                external_mesh.vertices.size() == 3u &&
                external_mesh.indices.size() == 3u,
                "staged external glTF geometry bin resolves into native MeshData");

            check(
                resolved.has_value() &&
                render::decode_gltf_base_color_texture(
                    *resolved, external_base_color, &resolve_error) &&
                external_base_color.valid() &&
                external_base_color.rgba8 == decoded_png.rgba8,
                "staged external glTF PNG auto material decodes exact source pixels");
        }


        const auto unsafe_model_path =
            root / "unsafe_resources.gltf";

        {
            std::ofstream output(
                unsafe_model_path,
                std::ios::binary | std::ios::trunc);
            output
                << R"json({"asset":{"version":"2.0"},"buffers":[{"byteLength":1,"uri":"../secret.bin"}],"meshes":[{"primitives":[{}]}]})json";
        }

        auto unsafe_asset = gltf_asset;
        unsafe_asset.source_path = unsafe_model_path;
        render::MeshData unsafe_mesh;
        std::string unsafe_error;

        check(
            !render::decode_gltf_mesh(
                unsafe_asset,
                unsafe_mesh,
                &unsafe_error) &&
            unsafe_error.find("URI") != std::string::npos,
            "glTF loader rejects external buffer path traversal");

        {
            std::ofstream output(
                unsafe_model_path,
                std::ios::binary | std::ios::trunc);
            output
                << R"json({"asset":{"version":"2.0"},"meshes":[{"primitives":[{"material":0}]}],"materials":[{"pbrMetallicRoughness":{"baseColorTexture":{"index":0}}}],"textures":[{"source":0}],"images":[{"uri":"../secret.png"}]})json";
        }

        render::DecodedTextureData unsafe_texture;
        unsafe_error.clear();

        check(
            !render::decode_gltf_base_color_texture(
                unsafe_asset,
                unsafe_texture,
                &unsafe_error) &&
            unsafe_error.find("URI") != std::string::npos,
            "glTF loader rejects external image path traversal");



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

        const auto obj_source =
            root / "textured_quad.obj";

        {
            std::ofstream output(
                obj_source,
                std::ios::binary |
                    std::ios::trunc);

            output
                << "# OBJ quad / n-gon test\n"
                << "v -1 -1 1\n"
                << "v 1 -1 1\n"
                << "v 1 1 1\n"
                << "v -1 1 1\n"
                << "vt 0 0\n"
                << "vt 1 0\n"
                << "vt 1 1\n"
                << "vt 0 1\n"
                << "vn 0 0 1\n"
                << "f 1/1/1 2/2/1 3/3/1 4/4/1\n";
        }

        render::ResolvedModelAsset
            obj_asset;

        obj_asset.guid =
            assets::AssetGuid::generate();
        obj_asset.metadata.format =
            ".obj";
        obj_asset.source_path =
            obj_source;

        render::MeshData
            decoded_obj;

        std::string obj_error;

        check(
            render::decode_obj_mesh(
                obj_asset,
                decoded_obj,
                &obj_error) &&
            decoded_obj.valid() &&
            decoded_obj.vertices.size() == 6u &&
            decoded_obj.indices ==
                std::vector<std::uint32_t>{
                    0u, 1u, 2u,
                    3u, 4u, 5u} &&
            decoded_obj.submeshes.size() == 1u &&
            decoded_obj.submeshes[0].first_index == 0u &&
            decoded_obj.submeshes[0].index_count == 6u &&
            decoded_obj.submeshes[0].material_slot ==
                render::kMeshMaterialUnassigned &&
            std::abs(
                decoded_obj.vertices[0]
                    .position.z + 1.0f) < 0.0001f &&
            std::abs(
                decoded_obj.vertices[0]
                    .normal.z + 1.0f) < 0.0001f &&
            std::abs(
                decoded_obj.vertices[0]
                    .uv.y - 1.0f) < 0.0001f &&
            std::abs(
                decoded_obj.vertices[1]
                    .uv.y - 0.0f) < 0.0001f &&
            std::abs(
                decoded_obj.bounds.center.z + 1.0f) < 0.0001f &&
            std::abs(
                decoded_obj.bounds.extents.x - 1.0f) < 0.0001f &&
            std::abs(
                decoded_obj.bounds.extents.y - 1.0f) < 0.0001f,
            "OBJ quad n-gon triangulates with UV normal bounds and handedness conversion");

        const auto negative_obj_source =
            root / "negative_indices.obj";

        {
            std::ofstream output(
                negative_obj_source,
                std::ios::binary |
                    std::ios::trunc);

            output
                << "v -1 -1 0\n"
                << "v 1 -1 0\n"
                << "v 1 1 0\n"
                << "v -1 1 0\n"
                << "f -4 -3 -2 -1\n";
        }

        obj_asset.source_path =
            negative_obj_source;

        render::MeshData
            negative_obj;

        obj_error.clear();

        check(
            render::decode_obj_mesh(
                obj_asset,
                negative_obj,
                &obj_error) &&
            negative_obj.valid() &&
            negative_obj.vertices.size() == 6u &&
            std::abs(
                negative_obj.vertices[0]
                    .normal.z + 1.0f) < 0.0001f &&
            std::abs(
                negative_obj.vertices[5]
                    .normal.z + 1.0f) < 0.0001f,
            "OBJ negative relative indices decode and missing normals generate flat face normals");

        const auto invalid_obj_source =
            root / "invalid_index.obj";

        {
            std::ofstream output(
                invalid_obj_source,
                std::ios::binary |
                    std::ios::trunc);

            output
                << "v 0 0 0\n"
                << "v 1 0 0\n"
                << "v 0 1 0\n"
                << "f 0 2 3\n";
        }

        obj_asset.source_path =
            invalid_obj_source;
        obj_error.clear();

        check(
            !render::decode_obj_mesh(
                obj_asset,
                negative_obj,
                &obj_error) &&
            obj_error.find(
                "position index") !=
                    std::string::npos,
            "OBJ decoder rejects zero face indices");

        const auto obj_descriptor =
            root / "obj_model.nasset";

        {
            std::ofstream output(
                obj_descriptor,
                std::ios::binary |
                    std::ios::trunc);

            output
                << "NENGINE_MODEL 1\n"
                << "FORMAT \".obj\"\n"
                << "SOURCE \"textured_quad.obj\"\n"
                << "SOURCE_BYTES "
                << std::filesystem::file_size(
                    obj_source)
                << "\n"
                << "END_MODEL\n";
        }

        assets::CachedArtifactSet
            obj_cached;

        obj_cached.fingerprint =
            "obj-v1";
        obj_cached.importer_id =
            "NEngine.Model";

        obj_cached.artifacts.push_back({
            obj_source,
            "source"
        });

        obj_cached.artifacts.push_back({
            obj_descriptor,
            "model-descriptor"
        });

        const auto obj_guid =
            assets::AssetGuid::generate();

        render::DecodedMeshCache
            obj_mesh_cache;

        const auto* cached_obj =
            obj_mesh_cache.load(
                obj_guid,
                obj_cached,
                &obj_error);

        check(
            cached_obj &&
            cached_obj->valid() &&
            cached_obj->indices.size() == 6u &&
            obj_mesh_cache.find(
                obj_guid) ==
                cached_obj,
            "OBJ model artifacts resolve through common AssetGuid decoded mesh cache");

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
                            gpu_texture.mip_levels() == 2u &&
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
                            gpu_asset->texture.mip_levels() == 2u &&
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

                        const auto material_gpu_root =
                            std::filesystem::temp_directory_path() /
                            ("nengine_gpu_material_" +
                             std::to_string(
                                 std::chrono::
                                     high_resolution_clock::
                                     now()
                                     .time_since_epoch()
                                     .count()));

                        std::filesystem::create_directories(
                            material_gpu_root);

                        const auto material_texture_source =
                            material_gpu_root /
                            "source.bmp";

                        const auto material_texture_descriptor =
                            material_gpu_root /
                            "texture.nasset";

                        const auto material_source =
                            material_gpu_root /
                            "source.nmat";

                        std::array<std::uint8_t, 58>
                            material_bmp{};

                        material_bmp[0] = 'B';
                        material_bmp[1] = 'M';
                        material_bmp[2] = 58u;
                        material_bmp[10] = 54u;
                        material_bmp[14] = 40u;
                        material_bmp[18] = 1u;
                        material_bmp[22] = 1u;
                        material_bmp[26] = 1u;
                        material_bmp[28] = 24u;
                        material_bmp[34] = 4u;

                        // One red BGR pixel + row padding.
                        material_bmp[54] = 0u;
                        material_bmp[55] = 0u;
                        material_bmp[56] = 255u;
                        material_bmp[57] = 0u;

                        {
                            std::ofstream output(
                                material_texture_source,
                                std::ios::binary |
                                    std::ios::trunc);

                            output.write(
                                reinterpret_cast<const char*>(
                                    material_bmp.data()),
                                static_cast<std::streamsize>(
                                    material_bmp.size()));
                        }

                        {
                            std::ofstream output(
                                material_texture_descriptor,
                                std::ios::binary |
                                    std::ios::trunc);

                            output
                                << "NENGINE_TEXTURE 1\n"
                                << "FORMAT \"bmp\"\n"
                                << "WIDTH 1\n"
                                << "HEIGHT 1\n"
                                << "COLOR_SPACE \"sRGB\"\n"
                                << "SOURCE \"source.bmp\"\n"
                                << "END_TEXTURE\n";
                        }

                        const auto material_texture_guid =
                            assets::AssetGuid::generate();

                        const auto imported_material_guid =
                            assets::AssetGuid::generate();

                        {
                            std::ofstream output(
                                material_source,
                                std::ios::binary |
                                    std::ios::trunc);

                            output
                                << "NENGINE_MATERIAL 1\n"
                                << "BASE_COLOR_TEXTURE \""
                                << material_texture_guid
                                    .to_string()
                                << "\"\n"
                                << "END_MATERIAL\n";
                        }

                        assets::CachedArtifactSet
                            material_texture_cached;

                        material_texture_cached.fingerprint =
                            "material-texture-v1";
                        material_texture_cached.importer_id =
                            "NEngine.Texture";
                        material_texture_cached.artifacts.push_back({
                            material_texture_source,
                            "source"
                        });
                        material_texture_cached.artifacts.push_back({
                            material_texture_descriptor,
                            "texture-descriptor"
                        });

                        assets::CachedArtifactSet
                            imported_material_cached;

                        imported_material_cached.fingerprint =
                            "material-v1";
                        imported_material_cached.importer_id =
                            "NEngine.Material";
                        imported_material_cached.artifacts.push_back({
                            material_source,
                            "source"
                        });

                        render::VulkanMaterialAssetCache
                            gpu_material_asset_cache;

                        check(
                            gpu_material_asset_cache.initialize(
                                loader,
                                instance,
                                device),
                            "Vulkan Material AssetGuid cache initializes for headless device");

                        const auto dependency_resolver =
                            [&](
                                assets::AssetGuid guid)
                                -> std::optional<
                                    assets::CachedArtifactSet> {

                                if (guid ==
                                    material_texture_guid) {
                                    return
                                        material_texture_cached;
                                }

                                return std::nullopt;
                            };

                        std::string material_asset_error;

                        const auto* imported_gpu_material =
                            gpu_material_asset_cache.load(
                                imported_material_guid,
                                imported_material_cached,
                                dependency_resolver,
                                &material_asset_error);

                        check(
                            imported_gpu_material &&
                            imported_gpu_material->valid() &&
                            gpu_material_asset_cache.find(
                                imported_material_guid) ==
                                imported_gpu_material &&
                            gpu_material_asset_cache.size() == 1u,
                            "nmat resolves texture artifacts into a real Vulkan sampled material descriptor");

                        // Full automatic model material path: model cache
                        // metadata -> glTF PBR image URI -> PNG decode ->
                        // Vulkan sampled texture and descriptor.
                        const auto gltf_auto_source =
                            material_gpu_root / "auto_textured.gltf";
                        const auto gltf_auto_descriptor =
                            material_gpu_root / "auto_model.nasset";
                        const auto gltf_auto_guid =
                            assets::AssetGuid::generate();

                        {
                            std::ofstream output(
                                gltf_auto_source,
                                std::ios::binary | std::ios::trunc);
                            output
                                << R"json({"asset":{"version":"2.0"},"meshes":[{"primitives":[{"material":0}]}],"materials":[{"pbrMetallicRoughness":{"baseColorTexture":{"index":0}}}],"textures":[{"source":0}],"images":[{"uri":"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAIAAAABCAYAAAD0In+KAAAAEUlEQVR42mP4z8Dwn+E/w38AEPgD/Tyf5vYAAAAASUVORK5CYII="}]})json";
                        }

                        {
                            std::ofstream output(
                                gltf_auto_descriptor,
                                std::ios::binary | std::ios::trunc);
                            output
                                << "NENGINE_MODEL 1\n"
                                << "FORMAT \".gltf\"\n"
                                << "SOURCE \"auto_textured.gltf\"\n"
                                << "SOURCE_BYTES "
                                << std::filesystem::file_size(
                                    gltf_auto_source)
                                << "\n"
                                << "END_MODEL\n";
                        }

                        assets::CachedArtifactSet gltf_auto_cached;
                        gltf_auto_cached.importer_id = "NEngine.Model";
                        gltf_auto_cached.fingerprint = "gltf-auto-v1";
                        gltf_auto_cached.artifacts.push_back({
                            gltf_auto_source, "source"
                        });
                        gltf_auto_cached.artifacts.push_back({
                            gltf_auto_descriptor, "model-descriptor"
                        });

                        const auto* auto_gpu_material =
                            gpu_material_asset_cache.load_gltf_base_color(
                                gltf_auto_guid,
                                gltf_auto_cached,
                                &material_asset_error);

                        check(
                            auto_gpu_material &&
                            auto_gpu_material->valid() &&
                            gpu_material_asset_cache.find_gltf_base_color(
                                gltf_auto_guid) == auto_gpu_material,
                            "GLB/glTF auto material uploads first PBR base-color image as Vulkan descriptor");

                        const auto multi_auto_source =
                            material_gpu_root / "multi_auto.gltf";
                        const auto multi_auto_descriptor =
                            material_gpu_root / "multi_auto.nasset";
                        const auto multi_auto_guid =
                            assets::AssetGuid::generate();

                        {
                            std::ofstream output(
                                multi_auto_source,
                                std::ios::binary | std::ios::trunc);
                            output
                                << R"json({"asset":{"version":"2.0"},"materials":[{"pbrMetallicRoughness":{"baseColorFactor":[1,0,0,1]}},{"pbrMetallicRoughness":{"baseColorFactor":[0,0.5,1,1]}}]})json";
                        }

                        {
                            std::ofstream output(
                                multi_auto_descriptor,
                                std::ios::binary | std::ios::trunc);
                            output
                                << "NENGINE_MODEL 1\n"
                                << "FORMAT \".gltf\"\n"
                                << "SOURCE \"multi_auto.gltf\"\n"
                                << "SOURCE_BYTES "
                                << std::filesystem::file_size(
                                    multi_auto_source)
                                << "\n"
                                << "END_MODEL\n";
                        }

                        assets::CachedArtifactSet multi_auto_cached;
                        multi_auto_cached.importer_id = "NEngine.Model";
                        multi_auto_cached.fingerprint = "multi-auto-v1";
                        multi_auto_cached.artifacts.push_back({
                            multi_auto_source, "source"
                        });
                        multi_auto_cached.artifacts.push_back({
                            multi_auto_descriptor, "model-descriptor"
                        });

                        const auto* auto_material_0 =
                            gpu_material_asset_cache.load_gltf_material(
                                multi_auto_guid,
                                0u,
                                multi_auto_cached,
                                &material_asset_error);

                        const auto* auto_material_1 =
                            gpu_material_asset_cache.load_gltf_material(
                                multi_auto_guid,
                                1u,
                                multi_auto_cached,
                                &material_asset_error);

                        check(
                            auto_material_0 &&
                            auto_material_0->valid() &&
                            auto_material_1 &&
                            auto_material_1->valid() &&
                            auto_material_0 != auto_material_1 &&
                            gpu_material_asset_cache.find_gltf_material(
                                multi_auto_guid, 0u) == auto_material_0 &&
                            gpu_material_asset_cache.find_gltf_material(
                                multi_auto_guid, 1u) == auto_material_1,
                            "automatic glTF material cache keeps independent Vulkan descriptors per material slot");

                        gpu_material_asset_cache.clear();

                        check(
                            gpu_material_asset_cache.find_gltf_base_color(
                                gltf_auto_guid) == nullptr &&
                            gpu_material_asset_cache.find_gltf_material(
                                multi_auto_guid, 0u) == nullptr &&
                            gpu_material_asset_cache.find_gltf_material(
                                multi_auto_guid, 1u) == nullptr &&
                            gpu_material_asset_cache.find(
                                imported_material_guid) == nullptr,
                            "automatic glTF and explicit nmat GPU materials invalidate together");


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

                        render::VulkanMeshResource
                            gpu_multi_material;

                        check(
                            decoded_multi_material_fixture.valid() &&
                            gpu_multi_material.create(
                                loader,
                                instance,
                                device,
                                decoded_multi_material_fixture) &&
                            gpu_multi_material.valid() &&
                            gpu_multi_material.index_count() == 6u &&
                            gpu_multi_material.submeshes().size() == 2u &&
                            gpu_multi_material.submeshes()[0].first_index == 0u &&
                            gpu_multi_material.submeshes()[0].index_count == 3u &&
                            gpu_multi_material.submeshes()[0].material_slot == 0u &&
                            gpu_multi_material.submeshes()[1].first_index == 3u &&
                            gpu_multi_material.submeshes()[1].index_count == 3u &&
                            gpu_multi_material.submeshes()[1].material_slot == 1u,
                            "Vulkan mesh resource preserves glTF submesh material ranges");


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
                        gpu_material_asset_cache.shutdown();

                        std::error_code material_cleanup_error;
                        std::filesystem::remove_all(
                            material_gpu_root,
                            material_cleanup_error);

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
