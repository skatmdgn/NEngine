#pragma once

#include <string>

#include "nengine/render/asset_resources.hpp"
#include "nengine/render/mesh_data.hpp"

namespace nengine::render {

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
// Material-library (.mtl) cooking is intentionally separate; decoded OBJ
// geometry currently receives an unassigned material slot.
bool decode_obj_mesh(
    const ResolvedModelAsset& asset,
    MeshData& mesh,
    std::string* error = nullptr);

} // namespace nengine::render
