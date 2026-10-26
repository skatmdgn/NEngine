#include "nengine/navigation/nav_scene.hpp"

#include <cstddef>
#include <string>
#include <vector>
#include "nengine/render/matrix.hpp"

namespace nengine::navigation {
namespace {
bool fail(std::string* error, std::string reason) {
    if (error) *error = std::move(reason);
    return false;
}
}

bool bake_scene_nav_mesh(const core::World& scene,
                         const SceneMeshResolver& resolve_mesh,
                         NavMesh& output,
                         const NavBakeOptions& options,
                         NavBakeReport* report,
                         std::string* error) {
    if (!resolve_mesh)
        return fail(error, "Scene NavMesh bake needs a mesh asset resolver");
    std::vector<NavBakeTriangle> triangles;
    for (const auto entity : scene.entities()) {
        if (!scene.active(entity)) continue;
        const auto* renderer = scene.get_component<render::MeshRenderer>(
            entity, render::mesh_renderer_type());
        if (!renderer || !renderer->enabled) continue;
        if (!renderer->mesh.valid())
            return fail(error, "Scene MeshRenderer has no valid mesh AssetGuid");
        const auto* mesh = resolve_mesh(renderer->mesh);
        if (!mesh || !mesh->valid())
            return fail(error, "Scene MeshRenderer mesh is missing or invalid");
        if (mesh->indices.size() / 3u > options.max_input_triangles ||
            triangles.size() > options.max_input_triangles -
                                    mesh->indices.size() / 3u)
            return fail(error, "Scene NavMesh triangle count exceeds bake budget");
        const auto model = render::world_matrix(scene, entity);
        for (std::size_t i = 0; i < mesh->indices.size(); i += 3u) {
            NavBakeTriangle face;
            for (std::size_t corner = 0; corner < 3u; ++corner) {
                const auto index = mesh->indices[i + corner];
                if (index >= mesh->vertices.size())
                    return fail(error, "Scene mesh triangle index out of range");
                face.vertices[corner] = render::transform_point(
                    model, mesh->vertices[index].position);
                if (!finite_position(face.vertices[corner]))
                    return fail(error, "Scene mesh world transform is invalid");
            }
            triangles.push_back(face);
        }
    }
    return bake_nav_triangles(triangles, output, options, report, error);
}
} // namespace nengine::navigation
