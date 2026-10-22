#include "nengine/navigation/nav_world.hpp"
#include <cmath>
#include <utility>
namespace nengine::navigation {
namespace {
bool finite_surface(const NavSurface& p) {
 return finite_position(p.size)&&p.size.x>0.0002f&&p.size.z>0.0002f&&
        p.area_mask!=0u&&std::isfinite(p.traversal_cost)&&
        p.traversal_cost>=1.f&&p.traversal_cost<=100000.f;
}
bool flat_transform(const core::Transform& t) {
 const auto q=t.local_rotation;
 return !t.parent.valid()&&finite_position(t.local_position)&&
        finite_position(t.local_scale)&&t.local_scale.x>0.f&&t.local_scale.z>0.f&&
        std::isfinite(q.x)&&std::isfinite(q.y)&&std::isfinite(q.z)&&std::isfinite(q.w)&&
        std::abs(q.x)<0.00001f&&std::abs(q.y)<0.00001f&&
        std::abs(q.z)<0.00001f&&std::abs(std::abs(q.w)-1.f)<0.00001f;
}
bool valid_agent(const NavAgentComponent& p) {
 return finite_position(p.destination)&&std::isfinite(p.speed)&&
        p.speed>=0.f&&p.speed<=100000.f&&
        std::isfinite(p.stopping_distance)&&p.stopping_distance>=0.f&&
        std::isfinite(p.radius)&&p.radius>=0.f&&p.area_mask!=0;
}
}
bool NavWorldSystem::bake(const core::World& world,NavWorldStats* stats,std::string* error){
 NavMesh next;
 std::size_t surfaces=0,connections=0;
 for(const auto entity:world.entities()){
  if(!world.active(entity))continue;
  const auto* source=world.get_component<NavSurface>(entity,nav_surface_type());
  if(!source||!source->enabled)continue;
  const auto* t=world.transform(entity);
  if(!t||!flat_transform(*t)||!finite_surface(*source)){
   if(error)*error="navigation requires finite unparented nonrotated surfaces";
   return false;
  }
  const float sx=0.5f*source->size.x*t->local_scale.x;
  const float sz=0.5f*source->size.z*t->local_scale.z;
  const auto p=t->local_position;
  if(!finite_position({p.x+sx,p.y,p.z+sz})||
     !finite_position({p.x-sx,p.y,p.z-sz})||
     !next.add_polygon({{p.x-sx,p.y,p.z-sz},{p.x+sx,p.y,p.z-sz},
                        {p.x+sx,p.y,p.z+sz},{p.x-sx,p.y,p.z+sz}},
                       nullptr,source->area_mask,source->traversal_cost)){
   if(error)*error="navigation polygon baking failed";
   return false;
  }
  ++surfaces;
 }
 for(NavId a=0;a<next.polygons().size();++a)
  for(NavId b=a+1;b<next.polygons().size();++b)
   if(next.connect(a,b))++connections;
 set_mesh(std::move(next));
 if(stats){*stats={};stats->surfaces=surfaces;stats->connected=connections;}
 if(error)error->clear();
 return true;
}
NavWorldStats NavWorldSystem::step(core::World& world,float dt){
 NavWorldStats result;
 if(!std::isfinite(dt)||dt<=0.f||dt>0.25f)return result;
 std::unordered_map<std::uint64_t,bool> active;
 for(const auto e:world.entities()){
  if(!world.active(e))continue;
  auto* component=world.get_component<NavAgentComponent>(e,nav_agent_type());
  auto* t=world.transform(e);
  if(!component||!component->enabled||!component->has_destination||
     !t||t->parent.valid()||!valid_agent(*component)||
     !finite_position(t->local_position))continue;
  active[e.value]=true;
  ++result.agents;
  auto& state=agents_[e.value];
  if(!state.assigned||state.target!=component->destination){
   const NavQuery query{component->area_mask,component->radius,false,4096u};
   const auto path=mesh_.find_path(t->local_position,component->destination,query);
   state.motion.stop();
   state.failed=!path.has_value();
   state.assigned=true;
   state.target=component->destination;
   state.motion.position=t->local_position;
   if(path&&!state.motion.assign(*path))state.failed=true;
  }
  if(state.failed){++result.failed_paths;continue;}
  state.motion.speed=component->speed;
  state.motion.stopping_distance=component->stopping_distance;
  if(!state.motion.update(dt)){++result.failed_paths;continue;}
  t->local_position=state.motion.position;
  if(state.motion.arrived)++result.arrived;
  else ++result.moving;
 }
 for(auto it=agents_.begin();it!=agents_.end();){
  if(!active.contains(it->first))it=agents_.erase(it);else ++it;
 }
 return result;
}
} // namespace nengine::navigation
