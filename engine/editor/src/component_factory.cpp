#include "nengine/editor/component_factory.hpp"

#include <algorithm>

namespace nengine::editor {

bool ComponentFactoryRegistry::register_factory(
    core::ComponentTypeId type,
    Factory factory) {

    if (type ==
            core::ComponentRegistry::invalid_type ||
        type ==
            core::World::transform_type ||
        !factory ||
        factories_.contains(type)) {
        return false;
    }

    factories_.emplace(
        type,
        std::move(factory));

    return true;
}

bool ComponentFactoryRegistry::unregister_factory(
    core::ComponentTypeId type) {

    return factories_.erase(type) != 0u;
}

bool ComponentFactoryRegistry::add(
    core::World& world,
    core::Entity entity,
    core::ComponentTypeId type) const {

    if (!world.is_alive(entity) ||
        world.has_component(
            entity,
            type)) {
        return false;
    }

    const auto it =
        factories_.find(type);

    return it != factories_.end() &&
        it->second(
            world,
            entity);
}

bool ComponentFactoryRegistry::contains(
    core::ComponentTypeId type) const noexcept {

    return factories_.contains(type);
}

std::vector<core::ComponentTypeId>
ComponentFactoryRegistry::types() const {

    std::vector<core::ComponentTypeId>
        result;

    result.reserve(
        factories_.size());

    for (const auto& [type, _] :
         factories_) {
        result.push_back(type);
    }

    std::sort(
        result.begin(),
        result.end());

    return result;
}

} // namespace nengine::editor
