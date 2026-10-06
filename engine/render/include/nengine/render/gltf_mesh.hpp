#pragma once

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

// Decode the first primitive's glTF 2.0 PBR base-color texture into
// top-left-origin RGBA8. Supports GLB bufferView images, data URI images,
// and external PNG/JPEG when present beside a glTF source. A missing
// base-color texture is a nonfatal false result for preview callers.
// Per-primitive materials, factors and advanced PBR are not yet cooked.
bool decode_gltf_base_color_texture(
    const ResolvedModelAsset& asset,
    DecodedTextureData& texture,
    std::string* error = nullptr);

} // namespace nengine::render
