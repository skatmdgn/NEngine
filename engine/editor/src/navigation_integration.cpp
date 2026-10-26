#include "nengine/editor/navigation_integration.hpp"
#include "nengine/navigation/components.hpp"
#include <cmath>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>
namespace nengine::editor {
bool register_navigation_property_access(PropertyAccessRegistry& properties) {
    using Component=navigation::NavAgentComponent;
    const auto type=navigation::nav_agent_type();
    bool ok=true;
    const auto field=[&](const char* name,core::PropertyKind kind,auto member) {
        using T=std::remove_cvref_t<decltype(std::declval<Component>().*member)>;
        ok=properties.register_property(
            type,name,kind,
            [type,member](const core::World& world,core::Entity entity)
                ->std::optional<core::PropertyValue> {
                const auto* a=world.get_component<Component>(entity,type);
                if(!a)return std::nullopt;
                if constexpr(std::is_same_v<T,float>)
                    return core::PropertyValue{static_cast<double>(a->*member)};
                else return core::PropertyValue{a->*member};
            },
            [type,member](core::World& world,core::Entity entity,
                          const core::PropertyValue& value) {
                auto* a=world.get_component<Component>(entity,type);
                if(!a)return false;
                if constexpr(std::is_same_v<T,float>){
                    const auto* number=std::get_if<double>(&value);
                    if(!number||!std::isfinite(*number)||
                       *number<0.||*number>1000.)return false;
                    a->*member=static_cast<float>(*number);
                }else{
                    const auto* parsed=std::get_if<T>(&value);
                    if(!parsed)return false;
                    if constexpr(std::is_same_v<T,core::Vec3>){
                        if(!navigation::finite_position(*parsed))return false;
                    }
                    a->*member=*parsed;
                }
                a->runtime_initialized=false;
                return true;
            })&&ok;
    };
    field("Enabled",core::PropertyKind::Boolean,&Component::enabled);
    field("Has Destination",core::PropertyKind::Boolean,&Component::has_destination);
    field("Destination",core::PropertyKind::Vec3,&Component::target);
    field("Speed",core::PropertyKind::Float,&Component::speed);
    field("Stopping Distance",core::PropertyKind::Float,&Component::stopping_distance);
    field("Radius",core::PropertyKind::Float,&Component::radius);
    return ok;
}
}