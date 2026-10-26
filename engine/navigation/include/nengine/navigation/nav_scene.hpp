#pragma once

#include <functional>
#include <string>

#include "nengine/core/world.hpp"
#include "nengine/navigation/nav_bake.hpp"
#include "nengine/render/components.hpp"
#include "nengine/render/mesh_data.hpp"

namespace nengine::navigation {

// The Editor (or an exporter) supplies already-imported MeshData by AssetGuid.
// No filesystem probing occurs here. All enabled MeshRenderer components of
// active Scene objects are collected in deterministic World order.
using SceneMeshResolver = std::function<const render::MeshData*(assets::AssetGuid)>;

// NAV-004/005: collect actual authored Scene meshes in world coordinates,
// including nested parent rotation, translation and non-uniform/mirrored scale.
// Invalid/missing mesh, malformed index, and invalid transform are hard
// errors. The previously published NavMesh and report survive any failure.
bool bake_scene_nav_mesh(const core::World& scene,
                         const SceneMeshResolver& resolve_mesh,
                         NavMesh& output,
                         const NavBakeOptions& options = {},
                         NavBakeReport* report = nullptr,
                         std::string* error = nullptr);

} // namespace nengine::navigation
