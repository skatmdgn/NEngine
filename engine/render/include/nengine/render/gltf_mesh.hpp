#pragma once

#include <cstddef>
#include <string>

#include "nengine/render/asset_resources.hpp"
#include "nengine/render/decoded_texture.hpp"
#include "nengine/render/mesh_data.hpp"

namespace nengine::render {

// First glTF 2.0 geometry path.
//
// Supported foundation:
// - .gltf JSON with data-URI buffers.
// - .gltf JSON with external buffers when they are available beside the source.
// - .glb 2.0 JSON + BIN chunks.
// - triangle primitives.
// - float POSITION/NORMAL/TEXCOORD_0 attributes.
// - unsigned byte/short/int indices.
// - non-interleaved or byteStride-interleaved buffer views.
// - multiple mesh primitives concatenated into one MeshData.
//
// Sparse accessors, morph targets, skinning and material cooking are intentionally
// handled by later milestones.
bool decode_gltf_mesh(
    const ResolvedModelAsset& asset,
    MeshData& mesh,
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
