#include "nengine/navigation/nav_world.hpp"
#include <cmath>
#include <vector>
#include <utility>
#include <optional>
#include <string_view>
#include <variant>
namespace nengine::navigation {
namespace {
const core::SerializedPropertyData* find(const core::SerializedComponentData& data,
                                          std::string_view name) {
 for(const auto& p:data.properties)if(p.name==name)return &p;
 return nullptr;
}
template<class T> bool read(const core::SerializedComponentData& data,
                            std::string_view name,T& out) {
 const auto* p=find(data,name);
 if(!p)return false;
 const auto* v=std::get_if<T>(&p->value);
 if(!v)return false;
 out=*v;return true;
}
core::PropertyFlags flags(){
 return core::PropertyFlags::Serializable|core::PropertyFlags::Editable;
}
}
bool register_navigation_components(core::ComponentRegistry& r){
 bool ok=r.register_type("NEngine.NavSurface","Navigation",true,false);
 for(const auto& [name,kind]:std::vector<std::pair<const char*,core::PropertyKind>>{
  {"Enabled",core::PropertyKind::Boolean},
  {"Size",core::PropertyKind::Vec3},
  {"Area Mask",core::PropertyKind::UnsignedInteger},
  {"Traversal Cost",core::PropertyKind::Float}})
  ok=r.register_property(nav_surface_type(),{name,kind,flags()})&&ok;
 ok=r.register_type("NEngine.NavAgent","Navigation",true,false)&&ok;
 for(const auto& [name,kind]:std::vector<std::pair<const char*,core::PropertyKind>>{
  {"Enabled",core::PropertyKind::Boolean},
  {"Has Destination",core::PropertyKind::Boolean},
  {"Destination",core::PropertyKind::Vec3},
  {"Speed",core::PropertyKind::Float},
  {"Stopping Distance",core::PropertyKind::Float},
  {"Radius",core::PropertyKind::Float},
  {"Area Mask",core::PropertyKind::UnsignedInteger}})
  ok=r.register_property(nav_agent_type(),{name,kind,flags()})&&ok;
 return ok;
}
bool register_navigation_serializers(core::ComponentSerializationRegistry& r){
 const bool a=r.register_codec({
  nav_surface_type(),1u,"NEngine.NavSurface",
  [](const core::World& world,core::Entity e)->std::optional<core::SerializedComponentData>{
   const auto* p=world.get_component<NavSurface>(e,nav_surface_type());
   if(!p)return std::nullopt;
   core::SerializedComponentData d;
   d.properties={
    {"Enabled",core::PropertyKind::Boolean,p->enabled},
    {"Size",core::PropertyKind::Vec3,p->size},
    {"Area Mask",core::PropertyKind::UnsignedInteger,static_cast<std::uint64_t>(p->area_mask)},
    {"Traversal Cost",core::PropertyKind::Float,static_cast<double>(p->traversal_cost)}
   };return d;
  },
  [](core::World& world,core::Entity e,const core::SerializedComponentData& d,
     std::string* error){
   NavSurface p;std::uint64_t mask=0;double cost=0;
   if(!read(d,"Enabled",p.enabled)||!read(d,"Size",p.size)||
      !read(d,"Area Mask",mask)||!read(d,"Traversal Cost",cost)||
      mask==0||mask>UINT32_MAX||!std::isfinite(cost)||
      cost<1.0||cost>100000.0||!finite_position(p.size)||
      p.size.x<=0.0002f||p.size.z<=0.0002f){
    if(error)*error="invalid NavSurface component";return false;
   }
   p.area_mask=static_cast<std::uint32_t>(mask);
   p.traversal_cost=static_cast<float>(cost);
   auto* target=world.get_component<NavSurface>(e,nav_surface_type());
   if(!target)target=world.add_component<NavSurface>(e,nav_surface_type());
   if(!target)return false;*target=p;return true;
  }
 });
 const bool b=r.register_codec({
  nav_agent_type(),1u,"NEngine.NavAgent",
  [](const core::World& world,core::Entity e)->std::optional<core::SerializedComponentData>{
   const auto* p=world.get_component<NavAgentComponent>(e,nav_agent_type());
   if(!p)return std::nullopt;
   core::SerializedComponentData d;
   d.properties={
    {"Enabled",core::PropertyKind::Boolean,p->enabled},
    {"Has Destination",core::PropertyKind::Boolean,p->has_destination},
    {"Destination",core::PropertyKind::Vec3,p->destination},
    {"Speed",core::PropertyKind::Float,static_cast<double>(p->speed)},
    {"Stopping Distance",core::PropertyKind::Float,static_cast<double>(p->stopping_distance)},
    {"Radius",core::PropertyKind::Float,static_cast<double>(p->radius)},
    {"Area Mask",core::PropertyKind::UnsignedInteger,static_cast<std::uint64_t>(p->area_mask)}
   };return d;
  },
  [](core::World& world,core::Entity e,const core::SerializedComponentData& d,
     std::string* error){
   NavAgentComponent p;std::uint64_t mask=0;
   double speed=0,stop=0,radius=0;
   if(!read(d,"Enabled",p.enabled)||!read(d,"Has Destination",p.has_destination)||
      !read(d,"Destination",p.destination)||!read(d,"Speed",speed)||
      !read(d,"Stopping Distance",stop)||!read(d,"Radius",radius)||
      !read(d,"Area Mask",mask)||mask==0||mask>UINT32_MAX||
      !std::isfinite(speed)||speed<0||speed>100000||
      !std::isfinite(stop)||stop<0||stop>100000||
      !std::isfinite(radius)||radius<0||radius>100000||
      !finite_position(p.destination)){
    if(error)*error="invalid NavAgent component";return false;
   }
   p.speed=static_cast<float>(speed);
   p.stopping_distance=static_cast<float>(stop);
   p.radius=static_cast<float>(radius);
   p.area_mask=static_cast<std::uint32_t>(mask);
   auto* target=world.get_component<NavAgentComponent>(e,nav_agent_type());
   if(!target)target=world.add_component<NavAgentComponent>(e,nav_agent_type());
   if(!target)return false;*target=p;return true;
  }
 });
 return a&&b;
}
} // namespace nengine::navigation
