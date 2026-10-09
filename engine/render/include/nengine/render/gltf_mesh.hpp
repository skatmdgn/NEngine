#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "nengine/render/asset_resources.hpp"
#include "nengine/render/decoded_texture.hpp"
#include "nengine/render/mesh_data.hpp"
#include "nengine/render/material_asset.hpp"

namespace nengine::render {

// First glTF 2.0 geometry path.
//
// Supported foundation:
// - .gltf JSON with data-URI buffers.
// - .gltf JSON with external buffers when they are available beside the source.
// - .glb 2.0 JSON + BIN chunks.
// - triangle primitives.
// - float and supported 8/16-bit quantized POSITION/NORMAL/TEXCOORD_0 attributes.
// - unsigned byte/short/int indices.
// - sparse VEC2/VEC3 attributes and sparse index overlays.
// - non-interleaved or byteStride-interleaved buffer views.
// - multiple mesh primitives with preserved submesh/material ranges.
//
// Morph targets and skinning remain later milestones. Referenced base-color
// materials are cooked by model_asset_importer into generated subassets.
bool decode_gltf_mesh(
    const ResolvedModelAsset& asset,
    MeshData& mesh,
    std::string* error = nullptr);

// Enumerate unique glTF materials[] indices actually referenced by mesh
// primitives. Used by import-time subasset cooking so unused authoring
// materials do not produce cache assets.
bool discover_gltf_material_slots(
    const ResolvedModelAsset& asset,
    std::vector<std::uint32_t>& material_slots,
    std::string* error = nullptr);

struct GltfPbrMaterialCookData {
    DecodedTextureData base_color{};
    std::optional<DecodedTextureData> normal{};
    std::optional<DecodedTextureData> metallic_roughness{};
    std::optional<DecodedTextureData> emissive{};
    std::optional<DecodedTextureData> occlusion{};

    float metallic_factor{1.0f};
    float roughness_factor{1.0f};
    core::Vec3 emissive_factor{};
    MaterialAlphaMode alpha_mode{
        MaterialAlphaMode::Opaque};
    float alpha_cutoff{0.5f};
    bool double_sided{false};
};

// Decode the supported glTF 2.0 metallic-roughness material payload used by
// import-time Material v2 cooking. Base color is always synthesized or
// decoded; optional maps retain their glTF color-space semantics.
bool decode_gltf_pbr_material(
    const ResolvedModelAsset& asset,
    std::size_t material_index,
    GltfPbrMaterialCookData& material,
    std::string* error = nullptr);

enum class GltfMaterialTextureKind {
    Normal,
    MetallicRoughness,
    Emissive,
    Occlusion
};

struct GltfMaterialProperties {
    float metallic_factor{1.0f};
    float roughness_factor{1.0f};
    float normal_scale{1.0f};
    float occlusion_strength{1.0f};
    core::Vec3 emissive_factor{};
    MaterialAlphaMode alpha_mode{
        MaterialAlphaMode::Opaque};
    float alpha_cutoff{0.5f};
    bool double_sided{false};
};

bool read_gltf_material_properties(
    const ResolvedModelAsset& asset,
    std::size_t material_index,
    GltfMaterialProperties& properties,
    std::string* error = nullptr);

// Decode one optional non-base-color glTF material texture. A false return
// means either the slot is absent or decode failed; error is populated only
// for malformed/unsupported data.
bool decode_gltf_material_texture(
    const ResolvedModelAsset& asset,
    std::size_t material_index,
    GltfMaterialTextureKind kind,
    DecodedTextureData& texture,
    std::string* error = nullptr);

// Decode one glTF material's PBR base-color input into top-left-origin
// RGBA8. Supports image-free baseColorFactor plus PNG/JPEG bufferView,
// data URI or staged external images.
bool decode_gltf_material_base_color_texture(
    const ResolvedModelAsset& asset,
    std::size_t material_index,
    DecodedTextureData& texture,
    std::string* error = nullptr);

// Compatibility helper: decode the first mesh primitive's material.
bool decode_gltf_base_color_texture(
    const ResolvedModelAsset& asset,
    DecodedTextureData& texture,
    std::string* error = nullptr);

} // namespace nengine::render
