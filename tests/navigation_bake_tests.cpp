#include "nengine/navigation/nav_bake.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

using namespace nengine;
using namespace nengine::navigation;

namespace {
int passed = 0;
int failed = 0;
void check(bool ok, const char* why) {
    if (ok) { ++passed; return; }
    ++failed;
    std::cerr << "FAILED " << why << "\n";
}
NavBakeTriangle tri(core::Vec3 a = {0,0,0},
                    core::Vec3 b = {1,0,0},
                    core::Vec3 c = {0,0,1},
                    std::uint32_t mask = 1u, float cost = 1.f) {
    return {{a,b,c},mask,cost};
}
std::vector<core::Vec3> square() {
    return {{0,0,0},{1,0,0},{1,0,1},{0,0,1}};
}
std::vector<std::uint32_t> square_indices() { return {0,1,2,0,2,3}; }
bool close(float x, float y) { return std::abs(x-y) < 0.0001f; }
} // namespace

int main() {
    NavMesh out;
    NavBakeReport report;
    std::string error;

    // 01. Empty input safely replaces stale polygons with an empty graph.
    check(bake_nav_triangles({}, out, {}, &report, &error) &&
          out.polygons().empty() && report.input_triangles == 0,
          "01 empty bake");

    // 02. A single authored triangle becomes one polygon.
    std::vector<NavBakeTriangle> source{tri()};
    check(bake_nav_triangles(source, out, {}, &report, &error) &&
          out.polygons().size() == 1 && out.contains(0,{.2f,0,.2f}),
          "02 authored triangle");

    // 03. Reverse winding is normalized without changing geometry.
    std::vector<NavBakeTriangle> reversed{tri({0,0,1},{1,0,0},{0,0,0})};
    check(bake_nav_triangles(reversed, out) &&
          out.contains(0,{.2f,0,.2f}),
          "03 winding normalization");

    // 04. Cooking is non-destructive to the author's input vertices.
    const auto original = reversed[0].vertices;
    check(bake_nav_triangles(reversed, out) &&
          reversed[0].vertices == original,
          "04 input remains unchanged");

    // 05. Indexed triangles share a properly connected diagonal.
    const auto vertices = square();
    const auto indices = square_indices();
    check(bake_indexed_nav_mesh(vertices,indices,out,{},1u,1.f,&report,&error) &&
          out.polygons().size()==2 && out.edges().size()==1,
          "05 indexed quad");

    // 06. Non-triplet index counts are rejected transactionally.
    const std::uint32_t short_indices[]{0,1};
    check(!bake_indexed_nav_mesh(vertices,short_indices,out) &&
          out.polygons().size()==2,
          "06 malformed index count");

    // 07. Out-of-range indices cannot index backing vertex memory.
    const std::uint32_t invalid_indices[]{0,1,999};
    check(!bake_indexed_nav_mesh(vertices,invalid_indices,out) &&
          out.polygons().size()==2,
          "07 index bounds");

    // 08. Unreferenced vertices need not be decoded/validated.
    auto extended = vertices;
    extended.push_back({std::numeric_limits<float>::quiet_NaN(),0,0});
    check(bake_indexed_nav_mesh(extended,indices,out) &&
          out.polygons().size()==2,
          "08 unused vertex ignored");

    // 09. Zero area masks are rejected, even on source triangles.
    check(!bake_nav_triangles(std::vector<NavBakeTriangle>{tri({}, {1,0,0}, {0,0,1},0)},out) &&
          out.polygons().size()==2,
          "09 zero area category");

    // 10. Invalid costs cannot be propagated to runtime path search.
    const auto nan = std::numeric_limits<float>::quiet_NaN();
    check(!bake_nav_triangles(std::vector<NavBakeTriangle>{tri({}, {1,0,0}, {0,0,1},1u,nan)},out) &&
          out.polygons().size()==2,
          "10 nonfinite traversal cost");

    // 11. Input limits prevent partial publication of an oversized graph.
    NavBakeOptions limited;
    limited.max_input_triangles = 1;
    check(!bake_indexed_nav_mesh(vertices,indices,out,limited) &&
          out.polygons().size()==2,
          "11 batch size budget");

    // 12. Nonfinite authored vertices are rejected.
    check(!bake_nav_triangles(std::vector<NavBakeTriangle>{
              tri({0,0,0},{1,0,0},{0,0,nan})},out) &&
          out.polygons().size()==2,
          "12 nonfinite coordinates");

    // 13. World-space coordinates are checked against engine bounds.
    check(!bake_nav_triangles(std::vector<NavBakeTriangle>{
              tri({1000001.f,0,0},{1,0,0},{0,0,1})},out),
          "13 coordinate bounds");

    // 14. Repeated/degenerate triangle vertices fail explicitly.
    check(!bake_nav_triangles(std::vector<NavBakeTriangle>{
              tri({0,0,0},{0,0,0},{0,0,1})},out) &&
          out.polygons().size()==2,
          "14 degenerate geometry");

    // 15. Vertical surfaces have no horizontal walkable projection.
    check(bake_nav_triangles(std::vector<NavBakeTriangle>{
              tri({0,0,0},{0,1,0},{0,0,1})},out,{},&report) &&
          out.polygons().empty() && report.filtered_small_area==1,
          "15 vertical triangle filter");

    // 16. Walkability respects configurable steep slope rejection.
    check(bake_nav_triangles(std::vector<NavBakeTriangle>{
              tri({0,0,0},{.1f,.2f,0},{0,0,1})},out,{},&report) &&
          out.polygons().empty() && report.filtered_slope==1,
          "16 steep slope filter");

    // 17. Shallow slopes with valid Y span remain navigable.
    check(bake_nav_triangles(std::vector<NavBakeTriangle>{
              tri({0,0,0},{1,.1f,0},{0,0,1})},out,{},&report) &&
          out.polygons().size()==1,
          "17 mild ramp");

    // 18. Per-face height span limits are not silently ignored.
    NavBakeOptions low_step;
    low_step.max_height_span=.1f;
    check(bake_nav_triangles(std::vector<NavBakeTriangle>{
              tri({0,0,0},{1,0,0},{0,.2f,1})},out,low_step,&report) &&
          out.polygons().empty() && report.filtered_height==1,
          "18 vertical-span filter");

    // 19. Tiny projected areas are filtered before NavMesh insertion.
    check(bake_nav_triangles(std::vector<NavBakeTriangle>{
              tri({0,0,0},{.01f,0,0},{0,0,.01f})},out,{},&report) &&
          out.polygons().empty() && report.filtered_small_area==1,
          "19 minimum footprint filter");

    // 20. Invalid slope/threshold configuration fails closed.
    NavBakeOptions bad;
    bad.max_slope_degrees=90.f;
    check(!bake_nav_triangles(source,out,bad),
          "20 option validation");

    // 21. Welding tolerances are bounded, not untrusted multipliers.
    bad={};
    bad.weld_tolerance=1.f;
    check(!bake_nav_triangles(source,out,bad),
          "21 weld parameter bounds");

    // 22. Area masks can exclude authoring surface classes.
    NavBakeOptions area_filter;
    area_filter.allowed_area_mask=1u;
    check(bake_nav_triangles(std::vector<NavBakeTriangle>{
              tri({0,0,0},{1,0,0},{0,0,1},2u)},out,area_filter,&report) &&
          out.polygons().empty() && report.filtered_area==1,
          "22 area category filter");

    // 23. Accepted area flags survive the cooking pipeline.
    check(bake_nav_triangles(std::vector<NavBakeTriangle>{
              tri({0,0,0},{1,0,0},{0,0,1},4u)},out) &&
          out.polygon(0)->area_mask==4u,
          "23 area attributes");

    // 24. Per-triangle traversal weights survive unchanged.
    check(bake_nav_triangles(std::vector<NavBakeTriangle>{
              tri({0,0,0},{1,0,0},{0,0,1},1u,3.f)},out) &&
          close(out.polygon(0)->traversal_cost,3.f),
          "24 traversal weights");

    // 25. Automatic adjacency generates a usable two-cell route.
    check(bake_indexed_nav_mesh(vertices,indices,out) &&
          out.edges().size()==1 &&
          out.find_path({.8f,0,.2f},{.2f,0,.8f}).has_value(),
          "25 routed triangle seam");

    // 26. The same baker supports intentionally disconnected faces.
    NavBakeOptions without_edges;
    without_edges.connect_shared_edges=false;
    check(bake_indexed_nav_mesh(vertices,indices,out,without_edges) &&
          out.edges().empty() &&
          !out.find_path({.8f,0,.2f},{.2f,0,.8f}),
          "26 disable adjacency");

    // 27. Spatially separate triangle islands cannot connect.
    check(bake_nav_triangles(std::vector<NavBakeTriangle>{
              tri(),tri({5,0,0},{6,0,0},{5,0,1})},out) &&
          out.polygons().size()==2 && out.edges().empty(),
          "27 detached islands");

    // 28. Explicitly quantized input can stitch near-duplicate seams.
    NavBakeOptions welded;
    welded.weld_tolerance=.001f;
    check(bake_nav_triangles(std::vector<NavBakeTriangle>{
              tri({0,0,0},{1.0002f,0,0},{0,0,1}),
              tri({1,0,0},{1,0,1},{0,0,1})},out,welded) &&
          out.edges().size()==1,
          "28 seam welding");

    // 29. Duplicate faces are rejected, leaving old graph intact.
    const auto previous=out.edges().size();
    check(!bake_nav_triangles(std::vector<NavBakeTriangle>{
              tri(),tri({0,0,1},{1,0,0},{0,0,0})},out) &&
          out.edges().size()==previous,
          "29 duplicate face safety");

    // 30. Three faces sharing the same edge are rejected transactionally.
    NavBakeReport unchanged{77,77,77,77,77,77,77};
    check(!bake_nav_triangles(std::vector<NavBakeTriangle>{
              tri(),
              tri({1,0,0},{0,0,0},{1,0,-1}),
              tri({0,0,0},{1,0,0},{.5f,0,-1})},
              out,{},&unchanged,&error) &&
          out.edges().size()==previous &&
          unchanged.input_triangles==77 &&
          !error.empty(),
          "30 non-manifold graph rejection");

    // Additional end-to-end regression: published baked graph serializes,
    // reloads and remains routable. This is not a separate implementation slice.
    check(bake_indexed_nav_mesh(vertices,indices,out), "roundtrip source bake");
    std::stringstream data;
    NavMesh roundtrip;
    check(write_nav_mesh(data,out,&error) &&
          read_nav_mesh(data,roundtrip,&error) &&
          roundtrip.find_path({.8f,0,.2f},{.2f,0,.8f}).has_value(),
          "baked surface .nnav persistence roundtrip");


    // NAV-008: explicitly resolve a T-junction instead of silently
    // disconnecting the two walkable halves of a shared boundary.
    {
        std::vector<NavBakeTriangle> junction{
            tri({0,0,0},{2,0,0},{0,0,1}),
            tri({1,0,0},{2,0,0},{1,0,-1})
        };
        NavBakeOptions option;
        check(bake_nav_triangles(junction,out,option) &&
              out.edges().empty(),
              "NAV-008: legacy mode retains original unconnected triangles");
        option.resolve_t_junctions=true;
        check(bake_nav_triangles(junction,out,option,&report,&error) &&
              out.edges().size()>=1u && report.input_triangles==2u &&
              report.accepted_triangles>=3u &&
              out.find_path({.5f,0,.15f},{1.5f,0,-.15f}),
              "NAV-008: T-junction is subdivided into routable full seams");
        option.max_input_triangles=2u;
        const auto saved=out.polygons().size();
        check(!bake_nav_triangles(junction,out,option,&report,&error) &&
              out.polygons().size()==saved && !error.empty(),
              "NAV-008: T-junction budget overflow is transactional");
        option.max_input_triangles=100u;
        std::vector<NavBakeTriangle> nonmanifold{
            tri(),tri({1,0,0},{0,0,0},{1,0,-1}),
            tri({0,0,0},{1,0,0},{.5f,0,-1})
        };
        check(!bake_nav_triangles(nonmanifold,out,option,&report,&error) &&
              out.polygons().size()==saved,
              "NAV-008: non-manifold seams remain rejected with subdivision");
    }
    if (failed) {
        std::cerr << "NEngineNavigationBakeTests: " << failed
                  << " failed / " << passed << " passed\n";
        return EXIT_FAILURE;
    }
    std::cout << "NEngineNavigationBakeTests: " << passed
              << " checks passed\n";
    return EXIT_SUCCESS;
}
