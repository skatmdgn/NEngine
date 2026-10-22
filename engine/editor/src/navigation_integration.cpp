#include "nengine/editor/navigation_integration.hpp"
#include "nengine/navigation/nav_world.hpp"
#include <cmath>
#include <type_traits>
#include <variant>
namespace nengine::editor {
namespace {
template<class C,class T>
bool property(PropertyAccessRegistry& props,core::ComponentTypeId id,
              std::string name,core::PropertyKind kind,T C::*field) {
 return props.register_property(id,name,kind,
  [id,field](const core::World& world,core::Entity e)
    ->std::optional<core::PropertyValue> {
    const auto* c=world.get_component<C>(e,id);
    if(!c)return std::nullopt;
    if constexpr(std::is_same_v<T,float>)
      return core::PropertyValue{static_cast<double>(c->*field)};
    else if constexpr(std::is_same_v<T,std::uint32_t>)
      return core::PropertyValue{static_cast<std::uint64_t>(c->*field)};
    else return core::PropertyValue{c->*field};
  },
  [id,field](core::World& world,core::Entity e,const core::PropertyValue& value) {
    auto* c=world.get_component<C>(e,id);
    if(!c)return false;
    if constexpr(std::is_same_v<T,float>){
      const auto* v=std::get_if<double>(&value);
      if(!v||!std::isfinite(*v)||*v<0.0||*v>100000.0)return false;
      c->*field=static_cast<float>(*v);return true;
    }else if constexpr(std::is_same_v<T,std::uint32_t>){
      const auto* v=std::get_if<std::uint64_t>(&value);
      if(!v||*v==0||*v>UINT32_MAX)return false;
      c->*field=static_cast<std::uint32_t>(*v);return true;
    }else if constexpr(std::is_same_v<T,core::Vec3>){
      const auto* v=std::get_if<core::Vec3>(&value);
      if(!v||!navigation::finite_position(*v))return false;
      c->*field=*v;return true;
    }else{
      const auto* v=std::get_if<T>(&value);
      if(!v)return false;
      c->*field=*v;return true;
    }
  });
}
}
bool register_navigation_integration(core::ComponentRegistry& registry,
 core::ComponentSerializationRegistry& codecs,PropertyAccessRegistry& props,
 ComponentFactoryRegistry& factories) {
 bool ok=navigation::register_navigation_components(registry);
 ok=navigation::register_navigation_serializers(codecs)&&ok;
 const auto surface=navigation::nav_surface_type(),agent=navigation::nav_agent_type();
 ok=factories.register_factory(surface,[](core::World& w,core::Entity e){
   return w.add_component<navigation::NavSurface>(e,navigation::nav_surface_type())!=nullptr;})&&ok;
 ok=factories.register_factory(agent,[](core::World& w,core::Entity e){
   return w.add_component<navigation::NavAgentComponent>(e,navigation::nav_agent_type())!=nullptr;})&&ok;
 using S=navigation::NavSurface;
 ok=property(props,surface,"Enabled",core::PropertyKind::Boolean,&S::enabled)&&ok;
 ok=property(props,surface,"Size",core::PropertyKind::Vec3,&S::size)&&ok;
 ok=property(props,surface,"Area Mask",core::PropertyKind::UnsignedInteger,&S::area_mask)&&ok;
 ok=property(props,surface,"Traversal Cost",core::PropertyKind::Float,&S::traversal_cost)&&ok;
 using A=navigation::NavAgentComponent;
 ok=property(props,agent,"Enabled",core::PropertyKind::Boolean,&A::enabled)&&ok;
 ok=property(props,agent,"Has Destination",core::PropertyKind::Boolean,&A::has_destination)&&ok;
 ok=property(props,agent,"Destination",core::PropertyKind::Vec3,&A::destination)&&ok;
 ok=property(props,agent,"Speed",core::PropertyKind::Float,&A::speed)&&ok;
 ok=property(props,agent,"Stopping Distance",core::PropertyKind::Float,&A::stopping_distance)&&ok;
 ok=property(props,agent,"Radius",core::PropertyKind::Float,&A::radius)&&ok;
 ok=property(props,agent,"Area Mask",core::PropertyKind::UnsignedInteger,&A::area_mask)&&ok;
 return ok;
}
} // namespace nengine::editor
