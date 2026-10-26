#include "nengine/navigation/nav_scene_agent.hpp"
#include <cmath>
#include "nengine/render/matrix.hpp"
namespace nengine::navigation {
std::size_t step_scene_agents(core::World& world,const NavMesh& nav,
                              float delta_seconds) {
    if(!std::isfinite(delta_seconds)||delta_seconds<=0.f||
       delta_seconds>10.f)return 0;
    std::size_t moved=0;
    for(const auto entity:world.entities()){
        if(!world.active(entity))continue;
        auto* a=world.get_component<NavAgentComponent>(entity,nav_agent_type());
        auto* transform=world.transform(entity);
        if(!a||!transform||!a->enabled)continue;
        auto& runner=a->runner;
        runner.position=render::transform_point(render::world_matrix(world,entity),{});
        runner.speed=a->speed;
        runner.stopping_distance=a->stopping_distance;
        if(!finite_position(runner.position)||!std::isfinite(a->radius)||
           a->radius<0.f)continue;
        if(!a->has_destination){
            runner.stop();a->runtime_initialized=false;continue;
        }
        const bool target_changed=!runner.destination ||
            *runner.destination!=a->target;
        if(!a->runtime_initialized||target_changed){
            NavQuery q;q.agent_radius=a->radius;
            if(!runner.set_destination(nav,a->target,q)){
                a->runtime_initialized=false;continue;
            }
            a->runtime_initialized=true;
        }
        if(!runner.update_navigation(nav,delta_seconds))continue;
        core::Vec3 local=runner.position;
        if(transform->parent.valid()){
            const auto inverse=render::inverse_affine(
                render::world_matrix(world,transform->parent));
            if(!inverse)continue;
            local=render::transform_point(*inverse,runner.position);
        }
        if(!finite_position(local))continue;
        transform->local_position=local;++moved;
        if(runner.arrived){
            a->has_destination=false;a->runtime_initialized=false;
        }
    }
    return moved;
}
}