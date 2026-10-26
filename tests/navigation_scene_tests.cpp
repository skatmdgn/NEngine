#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "nengine/core/world.hpp"
#include "nengine/navigation/nav_scene.hpp"

using namespace nengine;
namespace {
int failed = 0;
void check(bool value, const char* what) {
    if (!value) { ++failed; std::cerr << "FAIL: " << what << '\n'; }
}
render::MeshData floor_mesh() {
    render::MeshData m;
    for (auto p : std::vector<core::Vec3>{{0,0,0},{1,0,0},{1,0,1},{0,0,1}})
        m.vertices.push_back({p,{},{}});
    m.indices = {0,1,2,0,2,3};
    return m;
}
}
int main() {
    core::World world;
    auto parent = world.create("Model root");
    auto child = world.create("Walkable MeshRenderer");
    check(world.set_parent(child, parent), "NAV-005: nested mesh has parent");
    world.transform(parent)->local_position = {10,0,2};
    world.transform(parent)->local_scale = {2,1,2};
    world.transform(child)->local_position = {1,0,0};
    auto* renderer = world.add_component<render::MeshRenderer>(
        child, render::mesh_renderer_type());
    check(renderer != nullptr, "NAV-004: attach MeshRenderer to Scene World");
    renderer->mesh = assets::AssetGuid::from_string(
        "0123456789abcdef0123456789abcdef").value();
    auto mesh = floor_mesh();
    navigation::NavMesh baked;
    navigation::NavBakeReport report;
    std::string error;
    auto resolver = [&](assets::AssetGuid guid) -> const render::MeshData* {
        return guid == renderer->mesh ? &mesh : nullptr;
    };
    check(navigation::bake_scene_nav_mesh(
              world,resolver,baked,{},&report,&error) &&
          baked.polygons().size()==2 && baked.edges().size()==1 &&
          report.input_triangles==2,
          "NAV-004: collects actual Scene triangle mesh and bakes adjacency");
    check(baked.contains(0,{12.4f,0,2.4f}) ||
          baked.contains(1,{12.4f,0,2.4f}),
          "NAV-005: nested translation and non-uniform scale applied");
    check(!baked.contains(0,{.3f,0,.3f}),
          "NAV-005: mesh is not left in local coordinates");
    const auto previous = baked.polygons().size();
    check(!navigation::bake_scene_nav_mesh(
              world,[](assets::AssetGuid)->const render::MeshData*{return nullptr;},
              baked,{},&report,&error) &&
          !error.empty() && baked.polygons().size()==previous &&
          report.input_triangles==2,
          "NAV-004: missing imported mesh rolls back graph and report");
    renderer->enabled = false;
    check(navigation::bake_scene_nav_mesh(
              world,resolver,baked,{},&report,&error) &&
          baked.polygons().empty() && report.input_triangles==0,
          "NAV-004: disabled MeshRenderer is not baked");
    renderer->enabled = true;
    world.set_active(child,false);
    check(navigation::bake_scene_nav_mesh(
              world,resolver,baked,{},&report,&error) &&
          baked.polygons().empty(),
          "NAV-004: inactive Scene object is excluded");
    world.set_active(child,true);
    mesh.indices[2] = 55;
    check(!navigation::bake_scene_nav_mesh(
              world,resolver,baked,{},&report,&error) &&
          baked.polygons().empty(),
          "NAV-004: bad source mesh indices are rejected transactionally");
    mesh = floor_mesh();
    world.transform(parent)->local_scale={-2,1,2};
    check(navigation::bake_scene_nav_mesh(
              world,resolver,baked,{},&report,&error) &&
          baked.polygons().size()==2,
          "NAV-005: mirrored parent transform normalizes winding");
    if (failed) return EXIT_FAILURE;
    std::cout << "NEngineNavigationSceneTests PASS\n";
    return EXIT_SUCCESS;
}
