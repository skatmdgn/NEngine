#include "nengine/editor/property_access.hpp"

#include <utility>

namespace nengine::editor {

std::size_t PropertyAccessRegistry::KeyHash::operator()(
    const Key& key) const noexcept {

    const auto a =
        std::hash<core::ComponentTypeId>{}(
            key.component);

    const auto b =
        std::hash<std::string>{}(
            key.property);

    return a ^
        (b + 0x9e3779b97f4a7c15ull +
         (a << 6u) + (a >> 2u));
}

bool PropertyAccessRegistry::register_property(
    core::ComponentTypeId component,
    std::string property_name,
    core::PropertyKind kind,
    Getter getter,
    Setter setter) {

    if (component ==
            core::ComponentRegistry::invalid_type ||
        property_name.empty() ||
        !getter ||
        !setter) {
        return false;
    }

    Key key{
        component,
        std::move(property_name)
    };

    if (accessors_.contains(key)) {
        return false;
    }

    accessors_.emplace(
        std::move(key),
        Accessor{
            kind,
            std::move(getter),
            std::move(setter)
        });

    return true;
}

const PropertyAccessRegistry::Accessor*
PropertyAccessRegistry::find(
    core::ComponentTypeId component,
    std::string_view property_name) const noexcept {

    const Key key{
        component,
        std::string{property_name}
    };

    const auto it = accessors_.find(key);
    return it == accessors_.end()
        ? nullptr
        : &it->second;
}

std::optional<core::PropertyValue>
PropertyAccessRegistry::read(
    const core::World& world,
    core::Entity entity,
    core::ComponentTypeId component,
    std::string_view property_name) const {

    const auto* accessor =
        find(component, property_name);

    if (!accessor) return std::nullopt;
    return accessor->getter(world, entity);
}

bool PropertyAccessRegistry::write(
    core::World& world,
    core::Entity entity,
    core::ComponentTypeId component,
    std::string_view property_name,
    const core::PropertyValue& value) const {

    const auto* accessor =
        find(component, property_name);

    return accessor &&
        accessor->setter(
            world,
            entity,
            value);
}

} // namespace nengine::editor
