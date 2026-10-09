#include "nengine/editor/scripting_integration.hpp"

#include <string>
#include <variant>

#include "nengine/scripting/components.hpp"
#include "nengine/scripting/registration.hpp"

namespace nengine::editor {
namespace {

template <typename Component, typename Getter>
std::optional<core::PropertyValue>
read_component_property(
    const core::World& world,
    core::Entity entity,
    core::ComponentTypeId type,
    Getter getter) {

    const auto* component =
        world.get_component<Component>(
            entity,
            type);

    if (!component) {
        return std::nullopt;
    }

    return getter(
        *component);
}

template <typename Component, typename Setter>
bool write_component_property(
    core::World& world,
    core::Entity entity,
    core::ComponentTypeId type,
    const core::PropertyValue& value,
    Setter setter) {

    auto* component =
        world.get_component<Component>(
            entity,
            type);

    return
        component &&
        setter(
            *component,
            value);
}

} // namespace

bool register_scripting_integration(
    core::ComponentRegistry& components,
    core::ComponentSerializationRegistry&
        serialization,
    PropertyAccessRegistry& properties) {

    bool ok = true;

    ok =
        scripting::register_component_metadata(
            components) &&
        ok;

    ok =
        scripting::register_component_serializers(
            serialization) &&
        ok;

    const auto type =
        scripting::script_behaviour_type();

    ok =
        properties.register_property(
            type,
            "Enabled",
            core::PropertyKind::Boolean,
            [type](
                const core::World& world,
                core::Entity entity) {
                return
                    read_component_property<
                        scripting::ScriptBehaviour>(
                            world,
                            entity,
                            type,
                            [](const auto& behaviour) {
                                return
                                    core::PropertyValue{
                                        behaviour.enabled};
                            });
            },
            [type](
                core::World& world,
                core::Entity entity,
                const core::PropertyValue&
                    value) {
                return
                    write_component_property<
                        scripting::ScriptBehaviour>(
                            world,
                            entity,
                            type,
                            value,
                            [](auto& behaviour,
                               const auto& raw) {
                                const auto* typed =
                                    std::get_if<bool>(
                                        &raw);

                                if (!typed) {
                                    return false;
                                }

                                behaviour.enabled =
                                    *typed;

                                return true;
                            });
            }) &&
        ok;

    ok =
        properties.register_property(
            type,
            "Type Name",
            core::PropertyKind::String,
            [type](
                const core::World& world,
                core::Entity entity) {
                return
                    read_component_property<
                        scripting::ScriptBehaviour>(
                            world,
                            entity,
                            type,
                            [](const auto& behaviour) {
                                return
                                    core::PropertyValue{
                                        behaviour.type_name};
                            });
            },
            [type](
                core::World& world,
                core::Entity entity,
                const core::PropertyValue&
                    value) {
                return
                    write_component_property<
                        scripting::ScriptBehaviour>(
                            world,
                            entity,
                            type,
                            value,
                            [](auto& behaviour,
                               const auto& raw) {
                                const auto* typed =
                                    std::get_if<
                                        std::string>(
                                            &raw);

                                if (!typed) {
                                    return false;
                                }

                                behaviour.type_name =
                                    *typed;

                                return true;
                            });
            }) &&
        ok;

    return ok;
}

bool register_scripting_component_factories(
    ComponentFactoryRegistry& factories) {

    return
        factories.register_factory(
            scripting::script_behaviour_type(),
            [](core::World& world,
               core::Entity entity) {

                return
                    world.add_component<
                        scripting::ScriptBehaviour>(
                            entity,
                            scripting::
                                script_behaviour_type()) !=
                    nullptr;
            });
}

} // namespace nengine::editor
