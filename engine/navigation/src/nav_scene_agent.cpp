#include "nengine/navigation/nav_scene_agent.hpp"
#include <algorithm>
#include <cmath>
#include <vector>
#include "nengine/render/matrix.hpp"

namespace nengine::navigation {
namespace {
struct CrowdBody {
    core::Entity entity{};
    core::Vec3 position{};
    float radius{};
};
float horizontal_distance_squared(core::Vec3 a,core::Vec3 b) noexcept {
    const float dx=a.x-b.x,dz=a.z-b.z;
    return dx*dx+dz*dz;
}
// Continuous segment-circle check prevents fast agents tunneling through each
// other between fixed updates. Compare against committed preceding and current
// positions of subsequent agents in a deterministic World iteration.
bool clear_sweep(core::Vec3 from,core::Vec3 to,float radius,
                 const std::vector<CrowdBody>& crowd,core::Entity self) {
    const float dx=to.x-from.x,dz=to.z-from.z;
    const float length2=dx*dx+dz*dz;
    for(const auto& other:crowd) {
        if(other.entity==self||other.radius<=0.f)continue;
        const float limit=std::max(0.f,radius)+other.radius;
        const float start_distance=horizontal_distance_squared(from,other.position);
        // Already-overlapping actors may only separate; never move deeper.
        if(start_distance<limit*limit) {
            if(horizontal_distance_squared(to,other.position)<=start_distance)
                return false;
            continue;
        }
        const float dot=(other.position.x-from.x)*dx+
                        (other.position.z-from.z)*dz;
        const float t=length2>1e-10f?
            std::clamp(dot/length2,0.f,1.f):0.f;
        const core::Vec3 closest{
            from.x+dx*t,from.y,from.z+dz*t};
        if(horizontal_distance_squared(closest,other.position)<
           (limit+1e-4f)*(limit+1e-4f))
            return false;
    }
    return true;
}
}
std::size_t step_scene_agents(core::World& world,const NavMesh& nav,
                              float delta_seconds) {
    if(!std::isfinite(delta_seconds)||delta_seconds<=0.f||
       delta_seconds>10.f)return 0;
    std::vector<CrowdBody> crowd;
    for(const auto entity:world.entities()){
        if(!world.active(entity))continue;
        const auto* a=world.get_component<NavAgentComponent>(entity,nav_agent_type());
        if(!a||!a->enabled||!std::isfinite(a->radius)||a->radius<0.f)continue;
        const auto origin=render::transform_point(
            render::world_matrix(world,entity),{});
        if(!finite_position(origin))continue;
        crowd.push_back({entity,origin,a->radius});
    }
    std::size_t moved=0;
    for(const auto entity:world.entities()){
        if(!world.active(entity))continue;
        auto* a=world.get_component<NavAgentComponent>(entity,nav_agent_type());
        auto* transform=world.transform(entity);
        if(!a||!transform||!a->enabled)continue;
        auto& runner=a->runner;
        const auto original=render::transform_point(
            render::world_matrix(world,entity),{});
        runner.position=original;
        runner.speed=a->speed;
        runner.stopping_distance=a->stopping_distance;
        if(!finite_position(runner.position)||
           !std::isfinite(a->radius)||a->radius<0.f)continue;
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
        // Check a candidate without changing the authoritative agent until
        // collision avoidance and parent-local projection both succeed.
        auto proposed=runner;
        if(!proposed.update_navigation(nav,delta_seconds))continue;
        if(a->radius>0.f &&
           !clear_sweep(original,proposed.position,a->radius,crowd,entity)){
            const auto velocity=core::Vec3{
                proposed.position.x-original.x,0.f,
                proposed.position.z-original.z};
            const float length=std::hypot(velocity.x,velocity.z);
            bool diverted=false;
            if(length>0.0001f) {
                const float sidestep=std::min(
                    length,std::max(0.1f,2.f*a->radius));
                // Try both sides; a fixed left-first rule in travel direction
                // yields opposite offsets for opposing agents.
                for(const float sign:{1.f,-1.f}){
                    core::Vec3 alternate{
                        original.x-sign*velocity.z/length*sidestep,
                        original.y,
                        original.z+sign*velocity.x/length*sidestep};
                    NavQuery clearance;
                    clearance.agent_radius=a->radius;
                    if(!nav.locate(alternate,clearance)||
                       !clear_sweep(original,alternate,a->radius,crowd,entity))
                        continue;
                    proposed.position=alternate;
                    proposed.velocity={(alternate.x-original.x)/delta_seconds,0.f,
                                       (alternate.z-original.z)/delta_seconds};
                    diverted=true;
                    break;
                }
            }
            if(!diverted){
                runner.velocity={};
                continue;
            }
        }
        core::Vec3 local=proposed.position;
        if(transform->parent.valid()){
            const auto inverse=render::inverse_affine(
                render::world_matrix(world,transform->parent));
            if(!inverse)continue;
            local=render::transform_point(*inverse,proposed.position);
        }
        if(!finite_position(local))continue;
        const bool translated=horizontal_distance_squared(original,proposed.position)>1e-10f ||
             std::fabs(original.y-proposed.position.y)>1e-5f;
        runner=std::move(proposed);
        transform->local_position=local;
        for(auto& body:crowd)if(body.entity==entity){
            body.position=runner.position;
            break;
        }
        if(translated)++moved;
        if(runner.arrived){
            a->has_destination=false;a->runtime_initialized=false;
        }
    }
    return moved;
}
} // namespace nengine::navigation
