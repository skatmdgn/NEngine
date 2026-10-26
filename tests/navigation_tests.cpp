#include "nengine/navigation/nav_mesh.hpp"
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

using namespace nengine;
using namespace nengine::navigation;
namespace {
int failed=0,passed=0;
void check(bool result,const char* message){
    if(result){++passed;return;}
    ++failed;std::cerr<<"FAIL: "<<message<<"\n";
}
std::vector<core::Vec3> square(float x,float z=0,float width=1) {
    return {{x,0,z},{x+width,0,z},{x+width,0,z+1},{x,0,z+1}};
}
std::vector<core::Vec3> reversed_square(float x){
    return {{x,0,0},{x,0,1},{x+1,0,1},{x+1,0,0}};
}
NavMesh chain(){
    NavMesh mesh;
    for(int i=0;i<3;++i)mesh.add_polygon(square(static_cast<float>(i)));
    mesh.connect(0,1);mesh.connect(1,2);
    return mesh;
}
bool near(float a,float b,float epsilon=0.001f){
    return std::abs(a-b)<epsilon;
}
}
int main(){
    // 01. Convex authored polygons are accepted.
    NavMesh mesh;
    NavId first=invalid_nav_id;
    check(mesh.add_polygon(square(0),&first)&&mesh.polygons().size()==1,
          "01 convex navigation polygon insertion");
    // 02. Handles are deterministic contiguous identifiers.
    NavId second=invalid_nav_id;
    check(first==0 && mesh.add_polygon(square(1),&second)&&second==1,
          "02 stable polygon identifiers");
    // 03. Both valid polygon windings are supported.
    NavMesh clockwise;
    check(clockwise.add_polygon(reversed_square(0))&&
          clockwise.contains(0,{0.5f,0,0.5f}),"03 reversed winding");
    // 04. Nonconvex polygon geometry is rejected.
    check(!mesh.add_polygon({{0,0,0},{2,0,0},{1,0,0.5f},
                             {2,0,2},{0,0,2}}),"04 reject concave polygon");
    // 05. Degenerate and repeated edges are rejected.
    check(!mesh.add_polygon({{0,0,0},{1,0,0},{2,0,0}}) &&
          !mesh.add_polygon({{0,0,0},{0,0,0},{1,0,1}}),
          "05 reject degenerate and repeated corners");
    // 06. Nan and nonfinite geometry is rejected.
    const float nan=std::numeric_limits<float>::quiet_NaN();
    check(!mesh.add_polygon({{0,0,0},{1,0,0},{1,0,nan},{0,0,1}}),
          "06 reject nonfinite vertices");
    // 07. Zero area classification is invalid.
    check(!mesh.add_polygon(square(2),nullptr,0u),"07 area mask validation");
    // 08. Traversal costs have validated finite bounds.
    check(!mesh.add_polygon(square(2),nullptr,1u,0.f) &&
          !mesh.add_polygon(square(2),nullptr,1u,nan),
          "08 bounded positive area cost");
    // 09. Point containment accepts shared polygon boundaries.
    check(mesh.contains(0,{0.5f,0,0.5f}) &&
          mesh.contains(0,{1,0,0.5f}) &&
          !mesh.contains(0,{1.1f,0,0.5f}),
          "09 convex polygon containment and edges");
    // 10. Height-separated floors are not conflated.
    check(!mesh.contains(0,{0.5f,10,0.5f}) &&
          !mesh.add_polygon({{2,0,0},{3,0,0},{3,1,1},{2,0,1}}),
          "10 floor tolerance and planar validation");
    // 11. Connected polygons share a geometric edge.
    check(mesh.connect(0,1)&&mesh.edges().size()==1 &&
          !mesh.connect(0,0),"11 shared adjacency");
    // 12. Duplicate adjacency edges are rejected.
    check(!mesh.connect(1,0)&&mesh.edges().size()==1 &&
          !mesh.connect(1,99),"12 duplicate and invalid adjacency");
    // 13. Adjacent but spatially separated polygons cannot connect.
    NavId far=invalid_nav_id;
    mesh.add_polygon(square(5),&far);
    check(!mesh.connect(0,far),"13 non-sharing adjacency rejection");
    // 14. Shortest route through authored polygons.
    auto nav=chain();
    const auto simple=nav.find_path({.2f,0,.5f},{2.8f,0,.5f});
    check(simple&&simple->status==NavPathStatus::Complete &&
          simple->polygons==std::vector<NavId>({0,1,2}) &&
          simple->waypoints.size()==3&&
          near(simple->waypoints[1].x,1.5f),
          "14 deterministic shortest polygon corridor");
    // 15. Query area masks filter traversable polygons.
    nav.set_polygon_area(1,2u,1.f);
    check(!nav.find_path({.2f,0,.5f},{2.8f,0,.5f},
            NavQuery{1u,0.f,false,4096u}) &&
          nav.find_path({.2f,0,.5f},{2.8f,0,.5f},
            NavQuery{3u,0.f,false,4096u}).has_value(),
          "15 area classification path filter");
    // 16. Runtime polygon enablement changes queries.
    nav.set_polygon_enabled(1,false);
    check(!nav.find_path({.2f,0,.5f},{2.8f,0,.5f}) &&
          nav.set_polygon_enabled(1,true)&&
          nav.find_path({.2f,0,.5f},{2.8f,0,.5f}).has_value(),
          "16 enable or disable traversability");
    // 17. Area cost multipliers are finite and validated.
    check(nav.set_polygon_area(1,2u,4.f) &&
          !nav.set_polygon_area(1,0u,1.f) &&
          !nav.set_polygon_area(1,2u,nan) &&
          nav.polygon(1)->traversal_cost==4.f,
          "17 cost editing and validation");
    // 18. Dynamic obstacle intersects a navigable polygon.
    NavId blocker=invalid_nav_id;
    check(nav.add_obstacle({1.5f,0,.5f},.1f,&blocker) &&
          blocker==0&&nav.blocked(1),
          "18 circle blocker polygon intersection");
    // 19. Obstacle state is reversible and IDs stay stable.
    check(nav.set_obstacle_enabled(blocker,false)&&!nav.blocked(1)&&
          nav.set_obstacle_enabled(blocker,true)&&nav.blocked(1)&&
          nav.remove_obstacle(blocker)&&!nav.blocked(1),
          "19 obstacle state/removal");
    // 20. Agent radius inflates obstacle collision.
    nav.add_obstacle({1.5f,0,1.20f},.10f,&blocker);
    check(!nav.blocked(1,0.f)&&nav.blocked(1,.15f) &&
          !nav.locate({1.5f,0,.5f},NavQuery{3u,.15f,false,4096u}) &&
          nav.locate({1.5f,0,.5f},NavQuery{3u,0.f,false,4096u}),
          "20 obstacle agent clearance");
    // 21. Directed off-mesh links bypass disconnected polygons.
    NavMesh detached;
    detached.add_polygon(square(0));
    detached.add_polygon(square(4));
    NavId link=invalid_nav_id;
    check(detached.add_link(0,1,{.5f,0,.5f},{4.5f,0,.5f},1.f,false,&link)&&
          detached.find_path({.2f,0,.5f},{4.8f,0,.5f}).has_value(),
          "21 directed offmesh link traversal");
    // 22. Reverse traversal of a directed link is refused.
    check(!detached.find_path({4.8f,0,.5f},{.2f,0,.5f}) &&
          detached.set_link_enabled(link,false) &&
          !detached.find_path({.2f,0,.5f},{4.8f,0,.5f}),
          "22 reverse direction and link enablement");
    // 23. Bidirectional links allow the return journey.
    detached.add_link(0,1,{.5f,0,.5f},{4.5f,0,.5f},1.f,true);
    const auto back=detached.find_path({4.8f,0,.5f},{.2f,0,.5f});
    check(back&&back->used_offmesh_link&&back->waypoints.size()==4 &&
          back->status==NavPathStatus::Complete,
          "23 bidirectional offmesh jump endpoints");
    // 24. Partial paths lead as close as reachable, not invented goals.
    NavMesh partial;
    partial.add_polygon(square(0));partial.add_polygon(square(1));
    partial.add_polygon(square(5));partial.connect(0,1);
    const auto half=partial.find_path({.2f,0,.5f},{5.8f,0,.5f},
                                      NavQuery{~0u,0.f,true,4096u});
    check(half&&half->status==NavPathStatus::Partial &&
          half->polygons==std::vector<NavId>({0,1}) &&
          near(half->waypoints.back().x,1.5f),
          "24 best reachable partial path");
    // 25. Work budgets stop the search deterministically.
    check(!partial.find_path({.2f,0,.5f},{5.8f,0,.5f},
           NavQuery{~0u,0.f,true,1u}) &&
          !partial.find_path({.2f,0,.5f},{5.8f,0,.5f},
           NavQuery{~0u,0.f,false,4096u}),
          "25 bounded expansions and disconnected targets");
    // 26. Invalid query values fail safely.
    check(!nav.find_path({nan,0,0},{2.8f,0,.5f}) &&
          !nav.find_path({.2f,0,.5f},{2.8f,0,.5f},
            NavQuery{~0u,-1.f,false,4096u}) &&
          !nav.find_path({.2f,0,.5f},{2.8f,0,.5f},
            NavQuery{~0u,0.f,false,0u}),
          "26 invalid query input is rejected");
    if(!simple)return EXIT_FAILURE;
    // 27. Agent accepts a path without mutating input data.
    NavAgent agent;
    agent.position={.2f,0,.5f};
    const auto initial=simple->waypoints.size();
    check(agent.assign(*simple)&&!agent.arrived&&
          simple->waypoints.size()==initial,
          "27 agent route assignment");
    // 28. Agents progress through waypoints at bounded speed.
    agent.speed=1.f;agent.stopping_distance=0.f;
    check(agent.update(1.f)&&agent.position.x>.2f &&
          agent.position.x<2.f && agent.update(3.f)&&
          agent.arrived&&near(agent.position.x,2.8f),
          "28 time-stepped route movement and arrival");
    // 29. Stop and invalid deltas are safe, no old velocity retained.
    agent.position={.2f,0,.5f};
    agent.assign(*simple);
    check(!agent.update(nan)&&!agent.update(-1.f) &&
          agent.update(0.f)&&!agent.arrived, "29 invalid dt leaves active route");
    // 30. Bounded transactional mesh roundtrip including obstacles/links.
    NavMesh saved;
    saved.add_polygon(square(0));saved.add_polygon(square(1));
    saved.connect(0,1);
    saved.add_link(0,1,{.5f,0,.5f},{1.5f,0,.5f},1.f,true);
    saved.add_obstacle({.5f,0,2.f},.5f);
    saved.set_polygon_area(1,3u,2.f);
    std::stringstream stored;
    NavMesh restored;
    std::string error;
    const bool written=write_nav_mesh(stored,saved,&error);
    const bool loaded=read_nav_mesh(stored,restored,&error);
    check(written&&loaded&&restored.polygons().size()==2u &&
          restored.links().size()==1u&&restored.edges().size()==1u&&
          restored.obstacles().size()==1u &&
          restored.polygon(1)->area_mask==3u,
          "30 full navigation document roundtrip");
    const auto before=restored.polygons().size();
    std::stringstream malformed("NENGINE_NAVMESH 1\nPOLYGONS 1\nPOLYGON 0");
    check(!read_nav_mesh(malformed,restored,&error)&&
          restored.polygons().size()==before,
          "31 malformed documents are transactional");
    std::stringstream oversized;
    oversized<<std::string(4u*1024u*1024u+1u,'X');
    check(!read_nav_mesh(oversized,restored,&error)&&
          restored.polygons().size()==before,
          "32 huge documents reject pre-allocation");
    const auto root=std::filesystem::temp_directory_path() /
                    "nengine_navigation_test.nnav";
    check(save_nav_mesh(root,saved,&error)&&
          load_nav_mesh(root,restored,&error)&&
          restored.polygons().size()==2u,
          "33 disk save and load");
    std::error_code ignored;
    std::filesystem::remove(root,ignored);
    saved.clear();
    check(saved.polygons().empty()&&saved.links().empty()&&
          saved.obstacles().empty()&&saved.edges().empty(),
          "34 clearing the graph removes all state");

    // NAV-012: dynamic navigation edits automatically invalidate/replan routes.
    {
        auto dynamic=chain();
        NavAgent follower;
        follower.position={.2f,0,.5f};
        follower.speed=2.f;
        check(follower.set_destination(dynamic,{2.8f,0,.5f}) &&
              follower.destination.has_value(),
              "NAV-012: destination planned on live navigation graph");
        const auto revision=dynamic.revision();
        dynamic.set_polygon_enabled(1,false);
        check(dynamic.revision()>revision &&
              !follower.update_navigation(dynamic,.1f) &&
              follower.arrived && follower.route.empty() &&
              follower.destination.has_value(),
              "NAV-012: blocked polygon cancels stale route but keeps intent");
        dynamic.set_polygon_enabled(1,true);
        check(follower.update_navigation(dynamic,.1f) &&
              !follower.arrived && !follower.route.empty(),
              "NAV-012: unblocked polygon triggers automatic replan");
        NavId obstacle=invalid_nav_id;
        check(dynamic.add_obstacle({1.5f,0,.5f},.3f,&obstacle) &&
              !follower.update_navigation(dynamic,.1f) &&
              follower.arrived && follower.destination.has_value(),
              "NAV-012: new obstacle invalidates route");
        check(dynamic.remove_obstacle(obstacle) &&
              follower.update_navigation(dynamic,.1f) &&
              !follower.route.empty(),
              "NAV-012: obstacle removal automatically restores path");
        follower.stop();
        check(!follower.destination.has_value(),
              "NAV-012: manual stop clears destination");
    }
    if(failed){
        std::cerr<<"NEngineNavigationTests: "<<failed
                 <<" failed, "<<passed<<" passed\n";
        return EXIT_FAILURE;
    }
    std::cout<<"NEngineNavigationTests: "<<passed<<" checks passed\n";
    return EXIT_SUCCESS;
}
