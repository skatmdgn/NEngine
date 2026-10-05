#pragma once

#include <string>

#include "nengine/render/asset_resources.hpp"
#include "nengine/render/mesh_data.hpp"

namespace nengine::render {

// First glTF 2.0 geometry path.
//
// Supported foundation:
// - .gltf JSON with data-URI buffers.
// - .gltf JSON with external buffers when they are available beside the staged source.
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

} // namespace nengine::render
