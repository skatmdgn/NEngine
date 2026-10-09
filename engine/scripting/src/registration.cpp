#include "nengine/scripting/registration.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "nengine/scripting/components.hpp"

namespace nengine::scripting {
namespace {

const core::SerializedPropertyData*
find_property(
    const core::SerializedComponentData& data,
    std::string_view name) {

    for (const auto& property :
         data.properties) {

        if (property.name ==
            name) {
            return &property;
        }
    }

    return nullptr;
}

bool read_bool(
    const core::SerializedComponentData& data,
    std::string_view name,
    bool& value) {

    const auto* property =
        find_property(
            data,
            name);

    if (!property) {
        return false;
    }

    const auto* typed =
        std::get_if<bool>(
            &property->value);

    if (!typed) {
        return false;
    }

    value =
        *typed;

    return true;
}

bool read_string(
    const core::SerializedComponentData& data,
    std::string_view name,
    std::string& value) {

    const auto* property =
        find_property(
            data,
            name);

    if (!property) {
        return false;
    }

    const auto* typed =
        std::get_if<std::string>(
            &property->value);

    if (!typed) {
        return false;
    }

    value =
        *typed;

    return true;
}

} // namespace

bool register_component_metadata(
    core::ComponentRegistry& registry) {

    bool ok = true;

    ok =
        registry.register_type(
            "NEngine.ScriptBehaviour",
            "Scripting",
            true,
            false) &&
        ok;

    ok =
        registry.register_property(
            script_behaviour_type(),
            {
                "Enabled",
                core::PropertyKind::Boolean,
                core::PropertyFlags::Serializable |
                    core::PropertyFlags::Editable
            }) &&
        ok;

    ok =
        registry.register_property(
            script_behaviour_type(),
            {
                "Type Name",
                core::PropertyKind::String,
                core::PropertyFlags::Serializable |
                    core::PropertyFlags::Editable
            }) &&
        ok;

    return ok;
}

bool register_component_serializers(
    core::ComponentSerializationRegistry&
        registry) {

    return registry.register_codec({
        script_behaviour_type(),
        1u,
        "NEngine.ScriptBehaviour",
        [](const core::World& world,
           core::Entity entity)
            -> std::optional<
                core::SerializedComponentData> {

            const auto* behaviour =
                world.get_component<
                    ScriptBehaviour>(
                        entity,
                        script_behaviour_type());

            if (!behaviour) {
                return std::nullopt;
            }

            core::SerializedComponentData
                data;

            data.properties = {
                {
                    "Enabled",
                    core::PropertyKind::Boolean,
                    core::PropertyValue{
                        behaviour->enabled}
                },
                {
                    "Type Name",
                    core::PropertyKind::String,
                    core::PropertyValue{
                        behaviour->type_name}
                }
            };

            return data;
        },
        [](core::World& world,
           core::Entity entity,
           const core::SerializedComponentData&
               data,
           std::string* error) {

            ScriptBehaviour value;

            if (!read_bool(
                    data,
                    "Enabled",
                    value.enabled) ||
                !read_string(
                    data,
                    "Type Name",
                    value.type_name)) {

                if (error) {
                    *error =
                        "malformed NEngine.ScriptBehaviour data";
                }

                return false;
            }

            auto* behaviour =
                world.get_component<
                    ScriptBehaviour>(
                        entity,
                        script_behaviour_type());

            if (!behaviour) {
                behaviour =
                    world.add_component<
                        ScriptBehaviour>(
                            entity,
                            script_behaviour_type());
            }

            if (!behaviour) {
                return false;
            }

            *behaviour =
                std::move(
                    value);

            return true;
        }
    });
}

} // namespace nengine::scripting
