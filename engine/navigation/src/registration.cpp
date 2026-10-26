#include "nengine/navigation/registration.hpp"
#include "nengine/navigation/components.hpp"
#include <cmath>
#include <optional>
#include <string>
#include <utility>
#include <variant>
namespace nengine::navigation {
namespace {
const core::SerializedPropertyData* prop(
    const core::SerializedComponentData& data,const char* name) {
    for(const auto& p:data.properties)if(p.name==name)return &p;
    return nullptr;
}
template<class T>bool read(const core::SerializedComponentData& data,
                            const char* name,T& value){
    const auto* p=prop(data,name);
    if(!p)return false;
    const auto* typed=std::get_if<T>(&p->value);
    if(!typed)return false;
    value=*typed;return true;
}
bool valid(const NavAgentComponent& a){
    return std::isfinite(a.speed)&&a.speed>=0.f&&a.speed<=1000.f&&
        std::isfinite(a.stopping_distance)&&a.stopping_distance>=0.f&&
        a.stopping_distance<=100.f&&std::isfinite(a.radius)&&
        a.radius>=0.f&&a.radius<=100.f&&
        (!a.has_destination||finite_position(a.target));
}
}
bool register_component_metadata(core::ComponentRegistry& r){
    bool ok=r.register_type("NEngine.NavMeshAgent","Navigation",true,false);
    constexpr auto flags=core::PropertyFlags::Serializable|
                         core::PropertyFlags::Editable;
    for(const auto& [name,kind]:{
        std::pair{"Enabled",core::PropertyKind::Boolean},
        {"Has Destination",core::PropertyKind::Boolean},
        {"Destination",core::PropertyKind::Vec3},
        {"Speed",core::PropertyKind::Float},
        {"Stopping Distance",core::PropertyKind::Float},
        {"Radius",core::PropertyKind::Float}})
        ok=r.register_property(nav_agent_type(),{name,kind,flags})&&ok;
    return ok;
}
bool register_component_serializers(core::ComponentSerializationRegistry& r){
    return r.register_codec({
        nav_agent_type(),1,"NEngine.NavMeshAgent",
        [](const core::World& world,core::Entity entity)
            ->std::optional<core::SerializedComponentData> {
            const auto* a=world.get_component<NavAgentComponent>(
                entity,nav_agent_type());
            if(!a||!valid(*a))return std::nullopt;
            return core::SerializedComponentData{
                nav_agent_type(),1,"NEngine.NavMeshAgent",
                {{"Enabled",core::PropertyKind::Boolean,core::PropertyValue{a->enabled}},
                 {"Has Destination",core::PropertyKind::Boolean,core::PropertyValue{a->has_destination}},
                 {"Destination",core::PropertyKind::Vec3,core::PropertyValue{a->target}},
                 {"Speed",core::PropertyKind::Float,core::PropertyValue{double(a->speed)}},
                 {"Stopping Distance",core::PropertyKind::Float,core::PropertyValue{double(a->stopping_distance)}},
                 {"Radius",core::PropertyKind::Float,core::PropertyValue{double(a->radius)}}}
            };
        },
        [](core::World& world,core::Entity entity,
           const core::SerializedComponentData& data,std::string* error){
            NavAgentComponent staged;
            double speed=0.,stop=0.,radius=0.;
            const bool ok=read(data,"Enabled",staged.enabled)&&
                read(data,"Has Destination",staged.has_destination)&&
                read(data,"Destination",staged.target)&&
                read(data,"Speed",speed)&&
                read(data,"Stopping Distance",stop)&&
                read(data,"Radius",radius);
            if(ok){
                staged.speed=float(speed);
                staged.stopping_distance=float(stop);
                staged.radius=float(radius);
            }
            if(!ok||!valid(staged)){
                if(error)*error="NavMeshAgent properties are invalid";
                return false;
            }
            auto* out=world.get_component<NavAgentComponent>(
                entity,nav_agent_type());
            if(!out)out=world.add_component<NavAgentComponent>(
                entity,nav_agent_type());
            if(!out){
                if(error)*error="failed to attach NavMeshAgent";
                return false;
            }
            *out=staged;return true;
        }
    });
}
}