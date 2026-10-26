#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>
#include "nengine/navigation/components.hpp"
#include "nengine/navigation/nav_scene_agent.hpp"
#include "nengine/render/matrix.hpp"
using namespace nengine;
namespace{
int fails=0;
void check(bool cond,const char* name){
    if(!cond){++fails;std::cerr<<"FAIL "<<name<<"\n";}
}
std::vector<core::Vec3> square(float x){
    return {{x,0,0},{x+1,0,0},{x+1,0,2},{x,0,2}};
}
}
int main(){
    navigation::NavMesh mesh;
    for(int i=0;i<6;++i)check(mesh.add_polygon(square(float(i))),
                                  "NAV-013: build walkable broad corridor");
    for(int i=0;i<5;++i)check(mesh.connect(i,i+1),
                                  "NAV-013: join corridor");
    core::World world;
    auto first=world.create("Left");
    auto second=world.create("Right");
    auto* a=world.add_component<navigation::NavAgentComponent>(
        first,navigation::nav_agent_type());
    auto* b=world.add_component<navigation::NavAgentComponent>(
        second,navigation::nav_agent_type());
    // Pool insertion can resize storage, invalidating earlier pointers.
    a=world.get_component<navigation::NavAgentComponent>(
        first,navigation::nav_agent_type());
    b=world.get_component<navigation::NavAgentComponent>(
        second,navigation::nav_agent_type());
    check(a&&b,"NAV-013: two independent Scene NavMeshAgents");
    if(!a||!b)return EXIT_FAILURE;
    a->radius=.18f;b->radius=.18f;
    a->speed=2.f;b->speed=2.f;
    a->target={5.6f,0,1.f};b->target={.4f,0,1.f};
    a->has_destination=true;b->has_destination=true;
    world.transform(first)->local_position={.4f,0,1.f};
    world.transform(second)->local_position={5.6f,0,1.f};
    bool collision=false;
    bool both_moved=false;
    for(int i=0;i<180;++i){
        const auto moved=navigation::step_scene_agents(world,mesh,.05f);
        if(moved==2)both_moved=true;
        const auto p=render::transform_point(render::world_matrix(world,first),{});
        const auto q=render::transform_point(render::world_matrix(world,second),{});
        if(std::hypot(p.x-q.x,p.z-q.z)<.36f-0.002f)collision=true;
    }
    check(!collision,"NAV-013: simultaneous agents never overlap radii");
    check(both_moved,"NAV-013: both agents move concurrently");
    // Large delta must not tunnel directly through a stationary agent.
    core::World fast;
    auto fast_a=fast.create("Speed");
    auto stationary=fast.create("Stop");
    auto* fast_agent=fast.add_component<navigation::NavAgentComponent>(
        fast_a,navigation::nav_agent_type());
    auto* static_agent=fast.add_component<navigation::NavAgentComponent>(
        stationary,navigation::nav_agent_type());
    fast_agent=fast.get_component<navigation::NavAgentComponent>(
        fast_a,navigation::nav_agent_type());
    static_agent=fast.get_component<navigation::NavAgentComponent>(
        stationary,navigation::nav_agent_type());
    if(!fast_agent||!static_agent)return EXIT_FAILURE;
    fast_agent->radius=.2f;static_agent->radius=.2f;
    fast_agent->speed=10.f;fast_agent->has_destination=true;
    fast_agent->target={5.8f,0,1.f};
    fast.transform(fast_a)->local_position={.2f,0,1.f};
    fast.transform(stationary)->local_position={3.f,0,1.f};
    navigation::step_scene_agents(fast,mesh,.5f);
    const auto p=fast.transform(fast_a)->local_position;
    const auto q=fast.transform(stationary)->local_position;
    check(std::hypot(p.x-q.x,p.z-q.z)>=.4f-0.001f,
          "NAV-013: high-speed sweep rejects tunnelling");
    core::World many;
    for(int i=0;i<64;++i){
        const auto id=many.create("Crowd");
        auto* ag=many.add_component<navigation::NavAgentComponent>(
            id,navigation::nav_agent_type());
        ag->radius=.01f;ag->speed=1.f;ag->has_destination=true;
        ag->target={5.5f,0,.2f+float(i%16)*.1f};
        many.transform(id)->local_position={
            .2f+float(i/16)*.2f,0,.2f+float(i%16)*.1f};
    }
    const auto start=std::chrono::steady_clock::now();
    for(int frame=0;frame<120;++frame)
        navigation::step_scene_agents(many,mesh,.016f);
    const auto elapsed=std::chrono::duration<double>(
        std::chrono::steady_clock::now()-start).count();
    check(elapsed<5.,"NAV-013: 64 agents x 120 steps within bounded CPU time");
    if(fails)return EXIT_FAILURE;
    std::cout<<"NEngineNavigationCrowdTests PASS\n";
    return EXIT_SUCCESS;
}
