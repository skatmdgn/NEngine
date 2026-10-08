#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "nengine/render/asset_resources.hpp"
#include "nengine/render/mesh_data.hpp"

namespace nengine::render {

struct ObjMaterialSlot {
    std::uint32_t slot{0};
    std::string name{};
};

// Stable first-seen usemtl mapping shared by geometry decoding and material
// cooking. Repeated names reuse the same slot.
bool discover_obj_material_slots(
    const ResolvedModelAsset& asset,
    std::vector<ObjMaterialSlot>& materials,
    std::string* error = nullptr);

// Wavefront OBJ geometry foundation.
//
// Supported:
// - v / vt / vn records.
// - f corners in v, v/vt, v//vn or v/vt/vn form.
// - positive and negative relative indices.
// - triangles and polygon fan triangulation.
// - missing UVs and normals (flat normals are generated per triangle).
// - OBJ right-handed coordinates converted to NEngine left-handed space.
//
// Faces preserve usemtl groups as MeshSubmesh material slots. Material-library
// cooking is layered on the same slot map by model_asset_importer.
bool decode_obj_mesh(
    const ResolvedModelAsset& asset,
    MeshData& mesh,
    std::string* error = nullptr);

} // namespace nengine::render
